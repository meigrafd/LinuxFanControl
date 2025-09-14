// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using System.Net.Http;
using System.Net.Http.Json;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;

namespace LinuxFanControl.Gui.Services
{
    /// <summary>
    /// Minimal JSON-RPC 2.0 client over HTTP. No hard-coded schema assumptions.
    /// </summary>
    public sealed class JsonRpcClient : IAsyncDisposable
    {
        private readonly HttpClient _http;
        private readonly Uri _endpoint;

        public JsonRpcClient(string? baseUrl = null, HttpMessageHandler? handler = null)
        {
            _http = handler is null ? new HttpClient() : new HttpClient(handler, disposeHandler: false);
            _http.Timeout = TimeSpan.FromSeconds(15);
            _endpoint = new Uri(baseUrl ?? Environment.GetEnvironmentVariable("LFC_DAEMON_URL")
            ?? "http://127.0.0.1:28115/rpc");
        }

        public async Task<JsonElement?> RequestAsync(string method, object? @params = null, string? id = null, CancellationToken ct = default)
        {
            var payload = new
            {
                jsonrpc = "2.0",
                id = id ?? Guid.NewGuid().ToString("N"),
                method,
                @params
            };

            using var resp = await _http.PostAsJsonAsync(_endpoint, payload, ct);
            resp.EnsureSuccessStatusCode();
            var json = await resp.Content.ReadAsStringAsync(ct);
            using var doc = JsonDocument.Parse(json);

            if (doc.RootElement.TryGetProperty("error", out var err))
                throw new InvalidOperationException(err.GetRawText());

            if (doc.RootElement.TryGetProperty("result", out var res))
                return res;

            return null;
        }

        public async Task NotifyAsync(string method, object? @params = null, CancellationToken ct = default)
        {
            var payload = new { jsonrpc = "2.0", method, @params };
            using var req = new HttpRequestMessage(HttpMethod.Post, _endpoint)
            {
                Content = new StringContent(JsonSerializer.Serialize(payload), Encoding.UTF8, "application/json")
            };
            using var resp = await _http.SendAsync(req, ct);
            resp.EnsureSuccessStatusCode();
        }

        /// <summary>
        /// Simple helper to deserialize a JsonElement to T safely.
        /// </summary>
        public static T? Get<T>(JsonElement? el, JsonSerializerOptions? opts = null)
        {
            if (el is null) return default;
            return JsonSerializer.Deserialize<T>(el.Value.GetRawText(), opts);
        }

        /// <summary>
        /// Runs Auto-Setup (detect+calibrate) with incremental log stream.
        /// Expects daemon to push logs via polling method "setup.pollLogs".
        /// </summary>
        public async Task RunSetupAsync(IProgress<string> log, CancellationToken ct)
        {
            // kick off setup
            await NotifyAsync("detectCalibrate.start", ct: ct);

            // poll loop (daemon must implement "setup.pollLogs" returning array of strings until "done")
            while (!ct.IsCancellationRequested)
            {
                JsonElement? res = null;
                try { res = await RequestAsync("detectCalibrate.pollLogs", ct: ct); }
                catch (OperationCanceledException) { break; }

                if (res is null || res.Value.ValueKind != JsonValueKind.Object) break;

                if (res.Value.TryGetProperty("lines", out var lines) && lines.ValueKind == JsonValueKind.Array)
                {
                    foreach (var ln in lines.EnumerateArray())
                        if (ln.ValueKind == JsonValueKind.String) log.Report(ln.GetString() ?? "");
                }

                if (res.Value.TryGetProperty("done", out var done) && done.ValueKind == JsonValueKind.True)
                    break;

                await Task.Delay(250, ct);
            }
        }

        public Task CancelSetupAsync() => NotifyAsync("detectCalibrate.cancel");

        public async Task<T?> CallAsync<T>(string method, object? @params = null, CancellationToken ct = default)
        {
            var res = await RequestAsync(method, @params, ct: ct);
            return Get<T>(res);
        }

        public ValueTask DisposeAsync()
        {
            _http.Dispose();
            return ValueTask.CompletedTask;
        }

        // Helper factory for a background processing channel without type name conflicts.
        public static System.Threading.Channels.Channel<T> CreateUnbounded<T>() =>
        System.Threading.Channels.Channel.CreateUnbounded<T>();
    }
}
