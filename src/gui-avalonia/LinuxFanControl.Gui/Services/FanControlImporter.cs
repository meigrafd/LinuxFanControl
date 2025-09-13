// (c) 2025 LinuxFanControl contributors. MIT License.
// Importer for FanControl.Release-like JSON configs (best-effort heuristic).
// Maps curves/triggers to internal Profile/Channel model.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;

namespace LinuxFanControl.Gui.Services
{
    public static class FanControlImporter
    {
        public sealed class ImportResult
        {
            public AppConfig Config { get; init; } = new();
            public List<string> Warnings { get; } = new();
        }

        public static async Task<ImportResult> ImportAsync(string filePath)
        {
            var res = new ImportResult();
            using var doc = JsonDocument.Parse(await File.ReadAllTextAsync(filePath));

            var root = doc.RootElement;

            // Heuristic 1: profile name
            var profileName = root.TryGetProperty("Name", out var nameEl) && nameEl.ValueKind == JsonValueKind.String
            ? nameEl.GetString() ?? "Imported"
            : "Imported";

            // Heuristic 2: curves list (FanControl usually keeps per-controller objects)
            var channels = new List<Channel>();

            if (root.TryGetProperty("Controls", out var controls) && controls.ValueKind == JsonValueKind.Array)
            {
                foreach (var ctrl in controls.EnumerateArray())
                {
                    var ch = ParseControl(ctrl, res.Warnings);
                    if (ch is not null) channels.Add(ch);
                }
            }
            else
            {
                // Alternate schema: "Curves" + "Mappings"
                if (root.TryGetProperty("Curves", out var curves) && curves.ValueKind == JsonValueKind.Array)
                {
                    int idx = 1;
                    foreach (var c in curves.EnumerateArray())
                    {
                        var pts = ParsePoints(c, res.Warnings);
                        if (pts.Length == 0) continue;

                        var ch = new Channel
                        {
                            Name = c.TryGetProperty("Name", out var n) && n.ValueKind == JsonValueKind.String ? n.GetString() ?? $"Channel {idx}" : $"Channel {idx}",
                            Sensor = GuessSensorLabel(c),
                            Output = c.TryGetProperty("OutputId", out var o) && o.ValueKind == JsonValueKind.String ? o.GetString() ?? $"pwm{idx}" : $"pwm{idx}",
                            Mode = "Auto",
                            HysteresisC = c.TryGetProperty("Hysteresis", out var h) && h.TryGetDouble(out var hv) ? hv : 0.5,
                            ResponseTauS = c.TryGetProperty("Tau", out var t) && t.TryGetDouble(out var tv) ? tv : 2.0,
                            Curve = pts
                        };
                        idx++;
                        channels.Add(ch);
                    }
                }
                else
                {
                    res.Warnings.Add("No recognizable controls/curves in source file.");
                }
            }

            if (channels.Count == 0)
            {
                res.Warnings.Add("No channels imported.");
            }

            res.Config = new AppConfig
            {
                Version = ConfigService.CurrentVersion,
                Profiles = new[]
                {
                    new Profile
                    {
                        Name = profileName,
                        Channels = channels.ToArray()
                    }
                }
            };
            return res;
        }

        private static Channel? ParseControl(JsonElement ctrl, List<string> warnings)
        {
            // Known hints from FanControl.Release:
            // - "Type": "CurveController" / "MixedController" / "SyncController" / ...
            // - "Name"
            // - "Sensor" (description or id)
            // - "SpeedCurve": [{ X,Y }]
            // - "ResponseTime", "Hysteresis"
            // - "FanId" or "OutputId"
            var type = ctrl.TryGetProperty("Type", out var typeEl) && typeEl.ValueKind == JsonValueKind.String
            ? typeEl.GetString() ?? ""
            : "";

            var name = ctrl.TryGetProperty("Name", out var nameEl) && nameEl.ValueKind == JsonValueKind.String
            ? nameEl.GetString() ?? "Channel"
            : "Channel";

            var output = ctrl.TryGetProperty("FanId", out var fanEl) && fanEl.ValueKind == JsonValueKind.String
            ? fanEl.GetString() ?? ""
            : (ctrl.TryGetProperty("OutputId", out var outEl) && outEl.ValueKind == JsonValueKind.String ? outEl.GetString() ?? "" : "");

            if (string.IsNullOrEmpty(output))
            {
                warnings.Add($"Control '{name}': missing OutputId/FanId; assigning placeholder.");
                output = $"pwm_{Guid.NewGuid():N}";
            }

            var points = Array.Empty<CurvePoint>();
            if (ctrl.TryGetProperty("SpeedCurve", out var sc))
                points = ParsePoints(sc, warnings);
            else if (ctrl.TryGetProperty("Curve", out var c))
                points = ParsePoints(c, warnings);

            if (points.Length == 0)
            {
                // MixedController may not have direct points; create a safe default
                points = new[]
                {
                    new CurvePoint{ X = 20, Y = 20 },
                    new CurvePoint{ X = 40, Y = 40 },
                    new CurvePoint{ X = 60, Y = 70 },
                    new CurvePoint{ X = 80, Y = 100 },
                };
                warnings.Add($"Control '{name}': no curve points found, using a default curve.");
            }

            double hyst = 0.5;
            if (ctrl.TryGetProperty("Hysteresis", out var h) && h.TryGetDouble(out var hv)) hyst = hv;
            double tau = 2.0;
            if (ctrl.TryGetProperty("ResponseTime", out var rt) && rt.TryGetDouble(out var tv)) tau = tv;

            var sensor = GuessSensorLabel(ctrl);

            return new Channel
            {
                Name = name,
                Sensor = sensor,
                Output = output,
                Mode = "Auto",
                HysteresisC = hyst,
                ResponseTauS = tau,
                Curve = points
            };
        }

        private static CurvePoint[] ParsePoints(JsonElement el, List<string> warnings)
        {
            var list = new List<CurvePoint>();
            if (el.ValueKind == JsonValueKind.Array)
            {
                foreach (var p in el.EnumerateArray())
                {
                    if (TryReadXY(p, out var x, out var y))
                        list.Add(new CurvePoint { X = Clamp01(x) * 100.0, Y = Clamp01(y) * 100.0 });
                    else if (TryReadXYPercent(p, out x, out y))
                        list.Add(new CurvePoint { X = x, Y = y });
                }
            }
            else if (el.ValueKind == JsonValueKind.Object && TryReadXY(el, out var x, out var y))
            {
                list.Add(new CurvePoint { X = Clamp01(x) * 100.0, Y = Clamp01(y) * 100.0 });
            }
            list.Sort((a, b) => a.X.CompareTo(b.X));
            return list.ToArray();
        }

        // Some configs store 0..1 normalized; others 0..100 direct
        private static bool TryReadXY(JsonElement p, out double x, out double y)
        {
            x = y = 0;
            if (p.ValueKind != JsonValueKind.Object) return false;
            bool ok = false;
            if (p.TryGetProperty("X", out var xe) && xe.TryGetDouble(out x)) ok = true;
            if (p.TryGetProperty("Y", out var ye) && ye.TryGetDouble(out y)) ok = ok && true;
            return ok;
        }
        private static bool TryReadXYPercent(JsonElement p, out double x, out double y)
        {
            x = y = 0;
            if (p.ValueKind != JsonValueKind.Object) return false;
            bool ox = false, oy = false;
            if (p.TryGetProperty("XPercent", out var xe) && xe.TryGetDouble(out x)) ox = true;
            if (p.TryGetProperty("YPercent", out var ye) && ye.TryGetDouble(out y)) oy = true;
            return ox && oy;
        }

        private static double Clamp01(double v) => Math.Max(0, Math.Min(1, v));

        private static string GuessSensorLabel(JsonElement scope)
        {
            // try common fields
            if (scope.TryGetProperty("Sensor", out var s) && s.ValueKind == JsonValueKind.String)
                return s.GetString() ?? "Sensor";
            if (scope.TryGetProperty("SensorName", out var sn) && sn.ValueKind == JsonValueKind.String)
                return sn.GetString() ?? "Sensor";
            if (scope.TryGetProperty("Trigger", out var tr) && tr.ValueKind == JsonValueKind.String)
                return tr.GetString() ?? "Sensor";
            return "Sensor";
        }
    }
}
