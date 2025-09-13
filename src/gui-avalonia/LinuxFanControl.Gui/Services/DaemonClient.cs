// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using System.Net.Http;
using System.Net.Http.Json;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace LinuxFanControl.Gui.Services
{
    public interface IDaemonClient
    {
        Task<FanSnapshot[]> GetFansSnapshotAsync(CancellationToken ct);
        Task<bool> SetCurveAsync(string fanId, CurvePoint[] points);
    }

    public sealed record FanSnapshot(string Id, string Name, string Sensor, string Mode, int DutyPercent, int Rpm, double TempC);

    public static class DaemonClient
    {
        public static IDaemonClient Create() => new HttpDaemonClient();
    }

    internal sealed class HttpDaemonClient : IDaemonClient
    {
        private readonly HttpClient _http = new() { Timeout = TimeSpan.FromSeconds(3) };
        private readonly Uri _endpoint;

        public HttpDaemonClient()
        {
            _endpoint = new Uri(Environment.GetEnvironmentVariable("LFC_DAEMON_URL") ?? "http://127.0.0.1:28115/rpc");
        }

        public async Task<FanSnapshot[]> GetFansSnapshotAsync(CancellationToken ct)
        {
            try
            {
                var req = new { jsonrpc = "2.0", id = "snap", method = "telemetry.snapshot", @params = new object?[] { } };
                using var resp = await _http.PostAsJsonAsync(_endpoint, req, ct);
                resp.EnsureSuccessStatusCode();
                var json = await resp.Content.ReadAsStringAsync(ct);
                using var doc = JsonDocument.Parse(json);
                if (doc.RootElement.TryGetProperty("result", out var rs) && rs.ValueKind == JsonValueKind.Array)
                {
                    var list = System.Linq.Enumerable.ToList(System.Linq.Enumerable.Select(rs.EnumerateArray(), e =>
                        new FanSnapshot(
                            e.GetProperty("id").GetString() ?? "",
                            e.GetProperty("name").GetString() ?? "",
                            e.GetProperty("sensor").GetString() ?? "",
                            e.GetProperty("mode").GetString() ?? "Auto",
                            e.GetProperty("duty").GetInt32(),
                            e.GetProperty("rpm").GetInt32(),
                            e.GetProperty("temp").GetDouble()
                        )));
                    return list.ToArray();
                }
            }
            catch { }
            return Array.Empty<FanSnapshot>();
        }

        public async Task<bool> SetCurveAsync(string fanId, CurvePoint[] points)
        {
            try
            {
                var req = new { jsonrpc = "2.0", id = "curve:set", method = "setChannelCurve", @params = new object?[] { fanId, points } };
                using var resp = await _http.PostAsJsonAsync(_endpoint, req);
                resp.EnsureSuccessStatusCode();
                return true;
            }
            catch { return false; }
        }
    }
}
