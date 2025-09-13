// (c) 2025 LinuxFanControl contributors. MIT License.
// Comments in English, UI texts via external JSON (Locales/*.json).

using System;
using System.IO;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace LinuxFanControl.Gui.Services
{
    /// <summary>
    /// Minimal JSON-RPC 2.0 TCP client.
    /// - Endpoint default 127.0.0.1:8765 (override via env LFC_RPC=host:port or ~/.config/LinuxFanControl/daemon.json).
    /// - Line-delimited JSON for transport (ndjson).
    /// </summary>
    public sealed class RpcClient
    {
        public static RpcClient Instance { get; } = new();

        private string _host = "127.0.0.1";
        private int _port = 8765;

        private RpcClient()
        {
            var env = Environment.GetEnvironmentVariable("LFC_RPC");
            if (!string.IsNullOrWhiteSpace(env) && env.Contains(':'))
            {
                var parts = env.Split(':', 2);
                if (int.TryParse(parts[1], out var p)) { _host = parts[0]; _port = p; }
            }
            else
            {
                try
                {
                    var (file, _) = GetDaemonConfigPath();
                    if (File.Exists(file))
                    {
                        using var s = File.OpenRead(file);
                        using var doc = JsonDocument.Parse(s);
                        if (doc.RootElement.TryGetProperty("rpc", out var rpc))
                        {
                            if (rpc.TryGetProperty("host", out var h)) _host = h.GetString() ?? _host;
                            if (rpc.TryGetProperty("port", out var p) && p.TryGetInt32(out var pi)) _port = pi;
                        }
                    }
                }
                catch { /* ignore */ }
            }
        }

        public Task<JsonElement?> ListChannelsAsync(CancellationToken ct)
        => CallAsync("listChannels", JsonSerializer.SerializeToElement(new { }), ct);

        public Task<JsonElement?> DetectCalibrateAsync(CancellationToken ct)
        => CallAsync("detectCalibrate", JsonSerializer.SerializeToElement(new { quick = false }), ct);

        public async Task<JsonElement?> CallAsync(string method, JsonElement @params, CancellationToken ct)
        {
            using var tcp = new TcpClient { NoDelay = true };
            await tcp.ConnectAsync(_host, _port, ct);
            await using var ns = tcp.GetStream();
            using var reader = new StreamReader(ns, new UTF8Encoding(false), leaveOpen: true);
            await using var writer = new StreamWriter(ns, new UTF8Encoding(false), 8192, leaveOpen: true)
            { AutoFlush = true, NewLine = "\n" };

            var id = Guid.NewGuid().ToString("N");
            var req = new { jsonrpc = "2.0", id, method, @params };
            await writer.WriteLineAsync(JsonSerializer.Serialize(req).AsMemory(), ct);

            // Read until we receive our id
            while (!ct.IsCancellationRequested)
            {
                var line = await reader.ReadLineAsync();
                if (line is null) break;

                using var doc = JsonDocument.Parse(line);
                var root = doc.RootElement;

                if (root.TryGetProperty("id", out var rid) &&
                    rid.ValueKind == JsonValueKind.String &&
                    rid.GetString() == id)
                {
                    if (root.TryGetProperty("error", out var err))
                    {
                        var code = err.TryGetProperty("code", out var ce) ? ce.GetInt32() : 0;
                        var msg  = err.TryGetProperty("message", out var me) ? me.GetString() : "rpc error";
                        throw new InvalidOperationException($"RPC error {code}: {msg}");
                    }
                    if (root.TryGetProperty("result", out var res))
                    {
                        // detach result (copy)
                        var copy = JsonSerializer.Deserialize<JsonElement>(JsonSerializer.Serialize(res));
                        return copy;
                    }
                }
            }
            return null;
        }

        private static (string file, string dir) GetDaemonConfigPath()
        {
            string dir;
            if (OperatingSystem.IsLinux())
            {
                var xdg = Environment.GetEnvironmentVariable("XDG_CONFIG_HOME");
                dir = !string.IsNullOrEmpty(xdg)
                ? Path.Combine(xdg, "LinuxFanControl")
                : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".config", "LinuxFanControl");
            }
            else
            {
                dir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "LinuxFanControl");
            }
            return (Path.Combine(dir, "daemon.json"), dir);
        }
    }
}
