using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

public class JsonRpcClient
{
    private int _id = 1;

    public Func<string, string>? TransportSendReceive { get; set; }

    public JsonNode? Send(string method, JsonNode? @params = null)
    {
        var request = new JsonObject
        {
            ["jsonrpc"] = "2.0",
            ["id"] = _id++,
            ["method"] = method
        };

        if (@params != null)
            request["params"] = @params;

        if (TransportSendReceive == null)
            throw new InvalidOperationException("TransportSendReceive delegate is not set.");

        var responseJson = TransportSendReceive(request.ToJsonString());
        return JsonNode.Parse(responseJson);
    }
}
