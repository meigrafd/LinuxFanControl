#nullable enable
using System;
using System.Buffers;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace LinuxFanControl.Gui.Services
{
    public interface IRpcTransport : IAsyncDisposable
    {
        Task ConnectAsync(CancellationToken ct);
        Task SendAsync(ReadOnlyMemory<byte> payload, CancellationToken ct);
        Task<int> ReceiveAsync(Memory<byte> buffer, CancellationToken ct);
    }

    // TCP transport (fallback if unix socket not available)
    public sealed class TcpRpcTransport : IRpcTransport
    {
        private readonly string _host;
        private readonly int _port;
        private Socket? _sock;

        public TcpRpcTransport(string host, int port)
        {
            _host = host;
            _port = port;
        }

        public async Task ConnectAsync(CancellationToken ct)
        {
            _sock = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
            await _sock.ConnectAsync(new IPEndPoint(IPAddress.Loopback, _port), ct);
        }

        public async Task SendAsync(ReadOnlyMemory<byte> payload, CancellationToken ct)
        {
            if (_sock is null) throw new InvalidOperationException("Not connected");
            await _sock.SendAsync(payload, SocketFlags.None, ct);
        }

        public async Task<int> ReceiveAsync(Memory<byte> buffer, CancellationToken ct)
        {
            if (_sock is null) throw new InvalidOperationException("Not connected");
            return await _sock.ReceiveAsync(buffer, SocketFlags.None, ct);
        }

        public ValueTask DisposeAsync()
        {
            try { _sock?.Dispose(); } catch {}
            return ValueTask.CompletedTask;
        }
    }

    // JSON-RPC 2.0 client with batch support; newline-delimited frames
    public sealed class RpcClient : IAsyncDisposable
    {
        private readonly IRpcTransport _transport;

        public RpcClient(IRpcTransport transport)
        {
            _transport = transport;
        }

        public async Task ConnectAsync(CancellationToken ct = default) => await _transport.ConnectAsync(ct);

        public async Task<JsonElement> CallAsync(string method, object? @params, CancellationToken ct = default)
        {
            var id = Guid.NewGuid().ToString("N");
            var req = new Dictionary<string, object?> {
                ["jsonrpc"] = "2.0",
                ["id"] = id,
                ["method"] = method,
                ["params"] = @params
            };
            var json = JsonSerializer.Serialize(req);
            var msg = Encoding.UTF8.GetBytes(json + "\n");
            await _transport.SendAsync(msg, ct);
            var resp = await ReadFrameAsync(ct);
            return JsonDocument.Parse(resp).RootElement;
        }

        public async Task<JsonElement> CallBatchAsync(IEnumerable<object> batchArray, CancellationToken ct = default)
        {
            var json = JsonSerializer.Serialize(batchArray);
            var msg = Encoding.UTF8.GetBytes(json + "\n");
            await _transport.SendAsync(msg, ct);
            var resp = await ReadFrameAsync(ct);
            return JsonDocument.Parse(resp).RootElement;
        }

        private async Task<string> ReadFrameAsync(CancellationToken ct)
        {
            var buf = ArrayPool<byte>.Shared.Rent(64*1024);
            try
            {
                int n = await _transport.ReceiveAsync(buf, ct);
                if (n <= 0) throw new Exception("RPC receive returned 0");
                return Encoding.UTF8.GetString(buf, 0, n);
            }
            finally
            {
                ArrayPool<byte>.Shared.Return(buf);
            }
        }

        public ValueTask DisposeAsync() => _transport.DisposeAsync();
    }
}
