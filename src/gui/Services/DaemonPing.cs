using System.Text.Json.Nodes;

public class DaemonPing
{
    private readonly JsonRpcClient _rpc;

    public DaemonPing(JsonRpcClient rpc)
    {
        _rpc = rpc;
    }

    public bool IsAlive()
    {
        try
        {
            var response = _rpc.Send("ping");
            return response?["result"]?.ToString() == "pong";
        }
        catch
        {
            return false;
        }
    }
}
