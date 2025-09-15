using System.Text.Json.Nodes;

public class RpcBridge
{
    private readonly JsonRpcClient _client;

    public RpcBridge(JsonRpcClient client)
    {
        _client = client;
    }

    public void ApplyFanSpeed(string fanId, float percent)
    {
        var args = new JsonObject
        {
            ["id"] = fanId,
            ["percent"] = percent
        };

        _client.Send("setFanSpeed", args);
    }

    public void PushProfile(JsonNode profile)
    {
        var args = new JsonObject
        {
            ["profile"] = profile
        };

        _client.Send("applyProfile", args);
    }
}
