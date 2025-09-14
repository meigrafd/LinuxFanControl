#nullable enable
using System;
using System.Collections.Generic;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using LinuxFanControl.Gui.Models;

namespace LinuxFanControl.Gui.Services
{
    public sealed class DaemonClient
    {
        private readonly RpcClient _rpc;

        public DaemonClient(RpcClient rpc) => _rpc = rpc;

        public static async Task<DaemonClient?> TryConnectAsync(string mode = "tcp", string endpoint = "127.0.0.1:8484", CancellationToken ct = default)
        {
            try
            {
                IRpcTransport transport;
                if (mode == "tcp")
                {
                    var host = "127.0.0.1";
                    var port = 8484;
                    if (endpoint.Contains(":"))
                    {
                        var parts = endpoint.Split(':', 2);
                        host = parts[0];
                        port = int.Parse(parts[1]);
                    }
                    transport = new TcpRpcTransport(host, port);
                }
                else
                {
                    // Future: unix socket transport (requires targeting Linux-specific socket endpoint)
                    var host = "127.0.0.1"; var port = 8484;
                    transport = new TcpRpcTransport(host, port);
                }

                var rpc = new RpcClient(transport);
                await rpc.ConnectAsync(ct);
                return new DaemonClient(rpc);
            }
            catch
            {
                return null;
            }
        }

        public async Task<DetectionResult?> DetectCalibrateAsync(IProgress<string>? progress = null, CancellationToken ct = default)
        {
            var reqs = new List<object>
            {
                new Dictionary<string, object?>{{"jsonrpc","2.0"},{"id","1"},{"method","detectCalibrate"},{"params",new {}}},
                new Dictionary<string, object?>{{"jsonrpc","2.0"},{"id","2"},{"method","enumerate"},{"params",new {}}}
            };
            try
            {
                var resp = await _rpc.CallBatchAsync(reqs, ct);
                if (resp.ValueKind == JsonValueKind.Array)
                {
                    Sensor[] sensors = Array.Empty<Sensor>();
                    Fan[] fans = Array.Empty<Fan>();
                    foreach (var item in resp.EnumerateArray())
                    {
                        var id = item.GetProperty("id").GetString();
                        if (id == "2")
                        {
                            // expect result: { sensors: [...], pwms: [...] }
                            if (item.TryGetProperty("result", out var result))
                            {
                                if (result.TryGetProperty("sensors", out var sens))
                                {
                                    var list = new List<Sensor>();
                                    foreach (var s in sens.EnumerateArray())
                                    {
                                        list.Add(new Sensor
                                        {
                                            Id = s.GetProperty("id").GetString() ?? Guid.NewGuid().ToString(),
                                            Label = s.GetProperty("label").GetString() ?? "sensor",
                                            Type = s.TryGetProperty("type", out var tp) ? tp.GetString() ?? "Unknown" : "Unknown",
                                            Unit = s.TryGetProperty("unit", out var un) ? un.GetString() ?? "°C" : "°C",
                                            Value = s.TryGetProperty("value", out var val) && val.ValueKind==JsonValueKind.Number ? val.GetDouble() : 0.0
                                        });
                                    }
                                    sensors = list.ToArray();
                                }
                                if (result.TryGetProperty("pwms", out var pwm))
                                {
                                    var list = new List<Fan>();
                                    foreach (var f in pwm.EnumerateArray())
                                    {
                                        list.Add(new Fan
                                        {
                                            Id = f.GetProperty("id").GetString() ?? Guid.NewGuid().ToString(),
                                            Label = f.GetProperty("label").GetString() ?? "fan",
                                            Rpm = f.TryGetProperty("rpm", out var r) && r.ValueKind==JsonValueKind.Number ? r.GetInt32() : 0,
                                            Duty = f.TryGetProperty("duty", out var d) && d.ValueKind==JsonValueKind.Number ? d.GetDouble() : 0.0,
                                        });
                                    }
                                    fans = list.ToArray();
                                }
                            }
                        }
                    }
                    return new DetectionResult { Sensors = sensors, Fans = fans };
                }
                return null;
            }
            catch (Exception ex)
            {
                progress?.Report($"detect failed: {ex.Message}");
                return null;
            }
        }

        public async Task<(Sensor[] sensors, Fan[] fans)?> EnumerateAsync(CancellationToken ct = default)
        {
            try
            {
                var resp = await _rpc.CallAsync("enumerate", new { }, ct);
                if (resp.TryGetProperty("result", out var result))
                {
                    var sensors = new List<Sensor>();
                    var fans = new List<Fan>();
                    if (result.TryGetProperty("sensors", out var sens))
                    {
                        foreach (var s in sens.EnumerateArray())
                        {
                            sensors.Add(new Sensor
                            {
                                Id = s.GetProperty("id").GetString() ?? Guid.NewGuid().ToString(),
                                Label = s.GetProperty("label").GetString() ?? "sensor",
                                Type = s.TryGetProperty("type", out var tp) ? tp.GetString() ?? "Unknown" : "Unknown",
                                Unit = s.TryGetProperty("unit", out var un) ? un.GetString() ?? "°C" : "°C",
                                Value = s.TryGetProperty("value", out var val) && val.ValueKind==JsonValueKind.Number ? val.GetDouble() : 0.0
                            });
                        }
                    }
                    if (result.TryGetProperty("pwms", out var pwm))
                    {
                        foreach (var f in pwm.EnumerateArray())
                        {
                            fans.Add(new Fan
                            {
                                Id = f.GetProperty("id").GetString() ?? Guid.NewGuid().ToString(),
                                Label = f.GetProperty("label").GetString() ?? "fan",
                                Rpm = f.TryGetProperty("rpm", out var r) && r.ValueKind==JsonValueKind.Number ? r.GetInt32() : 0,
                                Duty = f.TryGetProperty("duty", out var d) && d.ValueKind==JsonValueKind.Number ? d.GetDouble() : 0.0,
                            });
                        }
                    }
                    return (sensors.ToArray(), fans.ToArray());
                }
            }
            catch { }
            return null;
        }
    }
}
