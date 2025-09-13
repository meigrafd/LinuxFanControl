// (c) 2025 LinuxFanControl contributors. MIT License.
// JSON-RPC-based daemon client with batch; mock fallback when daemon offline.

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace LinuxFanControl.Gui.Services
{
    public interface IDaemonClient : IDisposable
    {
        Task<FanSnapshot[]> GetFansSnapshotAsync(CancellationToken ct);
        Task<bool> SetCurveAsync(string fanId, CurvePoint[] points);

        // Setup: start detection+calibration and stream progress lines, cancelable.
        IAsyncEnumerable<string> RunSetupAsync(CancellationToken ct);
        Task CancelSetupAsync();
    }

    public sealed class DaemonClient : IDaemonClient
    {
        private readonly IDaemonClient _impl;

        private DaemonClient(IDaemonClient impl) => _impl = impl;

        public static IDaemonClient Create()
        {
            var url = Environment.GetEnvironmentVariable("LFC_DAEMON_URL") ?? "http://127.0.0.1:8765";
            try
            {
                return new DaemonClient(new RpcDaemonClient(url));
            }
            catch
            {
                return new MockDaemonClient();
            }
        }

        public void Dispose() => _impl.Dispose();
        public Task<FanSnapshot[]> GetFansSnapshotAsync(CancellationToken ct) => _impl.GetFansSnapshotAsync(ct);
        public Task<bool> SetCurveAsync(string fanId, CurvePoint[] points) => _impl.SetCurveAsync(fanId, points);
        public IAsyncEnumerable<string> RunSetupAsync(CancellationToken ct) => _impl.RunSetupAsync(ct);
        public Task CancelSetupAsync() => _impl.CancelSetupAsync();
    }

    // ---------------- RPC implementation ----------------
    internal sealed class RpcDaemonClient : IDaemonClient
    {
        private readonly JsonRpcClient _rpc;

        public RpcDaemonClient(string baseUrl)
        {
            _rpc = new JsonRpcClient(baseUrl);
        }

        public void Dispose() => _rpc.Dispose();

        public async Task<FanSnapshot[]> GetFansSnapshotAsync(CancellationToken ct)
        {
            // batch-friendly: backend should accept 'telemetry.snapshot'
            var res = await _rpc.CallAsync<FanSnapshot[]>("telemetry.snapshot", null, ct);
            return res ?? Array.Empty<FanSnapshot>();
        }

        public async Task<bool> SetCurveAsync(string fanId, CurvePoint[] points)
        {
            var ok = await _rpc.CallAsync<bool>("curve.set", new { fanId, points });
            return ok;
        }

        public async IAsyncEnumerable<string> RunSetupAsync([EnumeratorCancellation] CancellationToken ct)
        {
            // Start
            var session = await _rpc.CallAsync<string>("setup.start", null, ct);
            if (string.IsNullOrEmpty(session)) yield break;

            try
            {
                while (!ct.IsCancellationRequested)
                {
                    var chunk = await _rpc.CallAsync<string[]>("setup.poll", new { session }, ct);
                    if (chunk is { Length: > 0 })
                    {
                        foreach (var line in chunk)
                            yield return line;
                        // last line protocol hint
                        if (Array.Exists(chunk, l => l.StartsWith("DONE", StringComparison.OrdinalIgnoreCase)))
                            yield break;
                    }
                    await Task.Delay(300, ct);
                }
            }
            finally
            {
                await CancelSetupAsync();
            }
        }

        public Task CancelSetupAsync()
        => _rpc.CallAsync<bool>("setup.cancel");
    }

    // ---------------- Mock fallback (UI stays responsive) ----------------
    internal sealed class MockDaemonClient : IDaemonClient
    {
        private readonly Random _rng = new();
        private double _t;

        public void Dispose() { }

        public Task<FanSnapshot[]> GetFansSnapshotAsync(CancellationToken ct)
        {
            _t += 0.1;
            var arr = new FanSnapshot[4];
            for (int i = 0; i < arr.Length; i++)
            {
                var baseTemp = 37 + 10 * Math.Sin(_t + i);
                var duty = 25 + (int)(30 * (Math.Sin(_t * 0.9 + i) + 1));
                arr[i] = new FanSnapshot
                {
                    Id = $"fan{i+1}",
                    Name = $"Fan {i+1}",
                    Sensor = i switch { 0 => "CPU", 1 => "GPU", 2 => "Ambient", _ => "Water" },
                    Mode = "Auto",
                    DutyPercent = duty,
                    Rpm = 700 + _rng.Next(0, 1400),
                    TempC = Math.Round(baseTemp + _rng.NextDouble() * 0.6, 1)
                };
            }
            return Task.FromResult(arr);
        }

        public Task<bool> SetCurveAsync(string fanId, CurvePoint[] points) => Task.FromResult(true);

        public async IAsyncEnumerable<string> RunSetupAsync([EnumeratorCancellation] CancellationToken ct)
        {
            string[] phases = {
                "Detecting sensors & PWM…",
                "Inferring coupling (safe floor, pulses)…",
                "Calibrating (min & spin-up)…",
                "Restoring previous PWM states…",
                "Building default profiles…"
            };
            foreach (var p in phases)
            {
                yield return p;
                for (int i = 0; i < 10 && !ct.IsCancellationRequested; i++)
                {
                    yield return $"  {p} {i*10}%";
                    await Task.Delay(150, ct);
                }
            }
            yield return "DONE";
        }

        public Task CancelSetupAsync() => Task.CompletedTask;
    }

    // ---------------- DTOs ----------------
    public sealed class FanSnapshot
    {
        public string Id { get; set; } = "";
        public string Name { get; set; } = "";
        public string Sensor { get; set; } = "";
        public string Mode { get; set; } = "Auto";
        public int DutyPercent { get; set; }
        public int Rpm { get; set; }
        public double TempC { get; set; }
    }

    public sealed class CurvePoint
    {
        public double X { get; set; }
        public double Y { get; set; }
    }
}
