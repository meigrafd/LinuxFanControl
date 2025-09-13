using System.Diagnostics;
using System.Text;
using System.Text.Json;

namespace LinuxFanControl.Gui.Services;

public record RpcCall(string Method, JsonElement? Params = null);

public class RpcClient
{
    private Process? _p;
    private StreamWriter? _stdin;
    private StreamReader? _stdout;
    private int _id = 0;
    public RpcClient()
    {
        var daemon = Environment.GetEnvironmentVariable("LFC_DAEMON") ?? "../../../build/lfcd";
        var args = Environment.GetEnvironmentVariable("LFC_DAEMON_ARGS") ?? "";
        _p = new Process{
            StartInfo = new ProcessStartInfo{
                FileName = daemon,
                Arguments = args,
                RedirectStandardInput = true,
                RedirectStandardOutput = true,
                UseShellExecute = false
            }
        };
        _p.Start();
        _stdin = _p.StandardInput;
        _stdout = _p.StandardOutput;
    }

    public async Task<JsonElement> CallAsync(string method, object? paramObj = null)
    {
        var id = Interlocked.Increment(ref _id);
        using var ms = new MemoryStream();
        await using var w = new Utf8JsonWriter(ms);
        w.WriteStartObject();
        w.WriteString("jsonrpc","2.0");
        w.WriteNumber("id", id);
        w.WriteString("method", method);
        w.WritePropertyName("params");
        JsonSerializer.Serialize(w, paramObj ?? new { });
        w.WriteEndObject(); w.Flush();
        await _stdin!.WriteLineAsync(Encoding.UTF8.GetString(ms.ToArray()));
        var line = await _stdout!.ReadLineAsync();
        var doc = JsonDocument.Parse(line!);
        return doc.RootElement.GetProperty("result");
    }

    public async Task<Dictionary<string, JsonElement>> BatchAsync(List<RpcCall> calls)
    {
        var idmap = new Dictionary<int,string>();
        using var ms = new MemoryStream();
        await using var w = new Utf8JsonWriter(ms);
        w.WriteStartArray();
        foreach(var c in calls){
            var id = Interlocked.Increment(ref _id);
            idmap[id]=c.Method;
            w.WriteStartObject();
            w.WriteString("jsonrpc","2.0");
            w.WriteNumber("id", id);
            w.WriteString("method", c.Method);
            w.WritePropertyName("params");
            if(c.Params.HasValue) c.Params.Value.WriteTo(w); else w.WriteStartObject(); w.WriteEndObject();
            w.WriteEndObject();
        }
        w.WriteEndArray(); w.Flush();
        await _stdin!.WriteLineAsync(Encoding.UTF8.GetString(ms.ToArray()));
        var line = await _stdout!.ReadLineAsync();
        var doc = JsonDocument.Parse(line!);
        var res = new Dictionary<string, JsonElement>();
        foreach(var item in doc.RootElement.EnumerateArray()){
            var id = item.GetProperty("id").GetInt32();
            var key = idmap[id];
            res[key] = item.GetProperty("result");
        }
        return res;
    }
}
