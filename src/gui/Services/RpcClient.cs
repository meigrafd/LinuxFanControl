using System;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;
using System.IO;

namespace FanControl.Gui.Services;

public class RpcClient
{
    private readonly string _host;
    private readonly int _port;

    public RpcClient(string host = "127.0.0.1", int port = 9000)
    {
        _host = host;
        _port = port;
    }

    public async Task<string> SendAsync(string method, object? parameters = null)
    {
        var request = new
        {
            jsonrpc = "2.0",
            id = Guid.NewGuid().ToString(),
            method,
            @params = parameters
        };

        string json = JsonSerializer.Serialize(request);
        using var client = new TcpClient();

        await client.ConnectAsync(_host, _port);
        using var stream = client.GetStream();

        byte[] data = Encoding.UTF8.GetBytes(json + "\n");
        await stream.WriteAsync(data, 0, data.Length);

        using var reader = new StreamReader(stream, Encoding.UTF8);
        string response = await reader.ReadLineAsync() ?? "";

        return response;
    }

    public async Task<T?> CallAsync<T>(string method, object? parameters = null)
    {
        string raw = await SendAsync(method, parameters);

        using var doc = JsonDocument.Parse(raw);
        if (doc.RootElement.TryGetProperty("result", out var result))
        {
            return result.Deserialize<T>();
        }

        return default;
    }
}
