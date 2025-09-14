using System;
using System.IO;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Services;

public class RpcClient
{
    private readonly string _host;
    private readonly int _port;
    private int _idCounter = 1;

    public RpcClient(string host = "127.0.0.1", int port = 8765)
    {
        _host = host;
        _port = port;
    }

    public bool Ping()
    {
        try
        {
            var response = SendRequest("ping");
            return response is JsonObject obj && obj["result"]?.ToString() == "pong";
        }
        catch
        {
            return false;
        }
    }

    public JsonNode? GetSensors()
    {
        return SendRequest("getSensors");
    }

    public JsonNode? GetCurves()
    {
        return SendRequest("getCurves");
    }

    public void SetFanSpeed(string fanId, int value)
    {
        var parameters = new JsonObject
        {
            ["id"] = fanId,
            ["value"] = value
        };
        SendRequest("setFanSpeed", parameters);
    }

    private JsonNode? SendRequest(string method, JsonNode? parameters = null)
    {
        using var client = new TcpClient();
        client.Connect(_host, _port);

        using var stream = client.GetStream();
        using var writer = new StreamWriter(stream, Encoding.UTF8) { AutoFlush = true };
        using var reader = new StreamReader(stream, Encoding.UTF8);

        var request = new JsonObject
        {
            ["jsonrpc"] = "2.0",
            ["method"] = method,
            ["id"] = _idCounter++
        };

        if (parameters != null)
            request["params"] = parameters;

        string requestJson = request.ToJsonString();
        writer.WriteLine(requestJson);

        string responseJson = reader.ReadLine();
        if (string.IsNullOrWhiteSpace(responseJson))
            throw new IOException("Empty response from daemon");

        var response = JsonNode.Parse(responseJson);
        if (response is JsonObject obj && obj.ContainsKey("error"))
            throw new Exception($"Daemon error: {obj["error"]}");

        return response;
    }
}
