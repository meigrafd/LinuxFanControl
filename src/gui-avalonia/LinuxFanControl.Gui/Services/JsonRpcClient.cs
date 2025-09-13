// (c) 2025 LinuxFanControl contributors. MIT License.
// Minimal JSON-RPC 2.0 client with batching support over HTTP POST /rpc.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Net.Http;
using System.Net.Http.Json;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;

namespace LinuxFanControl.Gui.Services
{
    public sealed class JsonRpcClient : IDisposable
    {
        private readonly HttpClient _http;
        private readonly Uri _endpoint;
        private readonly Channel<JsonRpcRequest> _queue;
        private readonly CancellationTokenSource _cts = new();
        private readonly Task _worker;

        private static readonly JsonSerializerOptions _json = new(JsonSerializerDefaults.Web)
        {
            DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
            PropertyNamingPolicy = JsonNamingPolicy.CamelCase
        };

        public JsonRpcClient(string baseUrl)
        {
            _endpoint = new Uri(new Uri(baseUrl, UriKind.Absolute), "/rpc");
            _http = new HttpClient { Timeout = TimeSpan.FromSeconds(6) };
            _queue = Channel.CreateUnbounded<JsonRpcRequest>(new UnboundedChannelOptions
            {
                SingleReader = true,
                SingleWriter = false,
                AllowSynchronousContinuations = false
            });
            _worker = Task.Run(BatchWorkerAsync);
        }

        public async Task<T?> CallAsync<T>(string method, object? @params = null, CancellationToken ct = default)
        {
            var req = JsonRpcRequest.Create(method, @params);
            var tcs = new TaskCompletionSource<JsonRpcResponse>(TaskCreationOptions.RunContinuationsAsynchronously);
            req.Completion = tcs;

            if (!_queue.Writer.TryWrite(req))
                throw new InvalidOperationException("json-rpc queue backpressure");

            using var reg = ct.Register(() => tcs.TrySetCanceled(ct));
            var resp = await tcs.Task.ConfigureAwait(false);
            if (resp.Error is not null) throw new JsonRpcException(resp.Error);
            if (resp.Result is null) return default;
            return resp.Result.Deserialize<T>(_json);
        }

        public void Dispose()
        {
            _cts.Cancel();
            _http.Dispose();
        }

        // ----------------- batching worker -----------------
        private async Task BatchWorkerAsync()
        {
            var reader = _queue.Reader;
            var pending = new List<JsonRpcRequest>(32);

            while (!_cts.IsCancellationRequested)
            {
                try
                {
                    pending.Clear();

                    // wait first item
                    if (!await reader.WaitToReadAsync(_cts.Token).ConfigureAwait(false)) break;
                    if (reader.TryRead(out var first)) pending.Add(first);

                    // collect a short timeslice to batch more
                    var sw = System.Diagnostics.Stopwatch.StartNew();
                    while (sw.ElapsedMilliseconds < 8 && pending.Count < 48 && reader.TryRead(out var next))
                        pending.Add(next);

                    if (pending.Count == 0) continue;

                    // serialize
                    HttpResponseMessage httpResp;
                    if (pending.Count == 1)
                    {
                        var payload = pending[0].ToDto();
                        httpResp = await _http.PostAsJsonAsync(_endpoint, payload, _json, _cts.Token).ConfigureAwait(false);
                    }
                    else
                    {
                        var batch = new List<object>(pending.Count);
                        foreach (var r in pending) batch.Add(r.ToDto());
                        httpResp = await _http.PostAsJsonAsync(_endpoint, batch, _json, _cts.Token).ConfigureAwait(false);
                    }

                    httpResp.EnsureSuccessStatusCode();
                    var bytes = await httpResp.Content.ReadAsByteArrayAsync(_cts.Token).ConfigureAwait(false);

                    if (pending.Count == 1)
                    {
                        var resp = JsonSerializer.Deserialize<JsonRpcResponse>(bytes, _json);
                        pending[0].Completion?.TrySetResult(resp ?? JsonRpcResponse.ErrorFor(pending[0].Id, -32700, "Parse error"));
                    }
                    else
                    {
                        var respArr = JsonSerializer.Deserialize<JsonRpcResponse[]>(bytes, _json) ?? Array.Empty<JsonRpcResponse>();
                        var byId = new Dictionary<string, JsonRpcResponse>(StringComparer.Ordinal);
                        foreach (var r in respArr)
                            if (r.Id is not null) byId[r.Id] = r;
                            foreach (var req in pending)
                            {
                                if (req.Id is not null && byId.TryGetValue(req.Id, out var match))
                                    req.Completion?.TrySetResult(match);
                                else
                                    req.Completion?.TrySetResult(JsonRpcResponse.ErrorFor(req.Id, -32603, "Missing response"));
                            }
                    }
                }
                catch (Exception ex)
                {
                    // fail all pending
                    foreach (var r in pending)
                        r.Completion?.TrySetException(ex);
                    await Task.Delay(50, _cts.Token).ConfigureAwait(false);
                }
            }
        }

        // ----------------- DTOs -----------------
        private sealed class JsonRpcRequest
        {
            public string Jsonrpc { get; init; } = "2.0";
            public string? Id { get; init; }
            public string Method { get; init; } = "";
            public object? Params { get; init; }

            [System.Text.Json.Serialization.JsonIgnore] public TaskCompletionSource<JsonRpcResponse>? Completion { get; set; }

            public static JsonRpcRequest Create(string method, object? @params)
            => new() { Id = Guid.NewGuid().ToString("N"), Method = method, Params = @params };

            public object ToDto() => new { jsonrpc = Jsonrpc, id = Id, method = Method, @params = Params };
        }

        private sealed class JsonRpcResponse
        {
            public string Jsonrpc { get; set; } = "2.0";
            public string? Id { get; set; }
            public JsonElement? Result { get; set; }
            public JsonRpcError? Error { get; set; }

            public static JsonRpcResponse ErrorFor(string? id, int code, string message)
            => new() { Id = id, Error = new JsonRpcError { Code = code, Message = message } };
        }

        public sealed class JsonRpcError
        {
            public int Code { get; set; }
            public string Message { get; set; } = "";
            public JsonElement? Data { get; set; }

            public override string ToString() => $"{Code}: {Message}";
        }

        public sealed class JsonRpcException : Exception
        {
            public JsonRpcError Error { get; }
            public JsonRpcException(JsonRpcError error) : base(error.ToString()) => Error = error;
        }
    }
}
