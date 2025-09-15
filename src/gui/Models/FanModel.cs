using System.Text.Json.Nodes;

public class FanModel
{
    public string Id { get; }
    public string Label { get; }
    public int Rpm { get; }
    public string Mode { get; }

    public FanModel(string id, string label, int rpm, string mode)
    {
        Id = id;
        Label = label;
        Rpm = rpm;
        Mode = mode;
    }

    public static List<FanModel> FromJson(JsonNode? node)
    {
        var list = new List<FanModel>();
        if (node is not JsonArray arr) return list;

        foreach (var item in arr)
        {
            var id = item?["id"]?.ToString() ?? "";
            var label = item?["label"]?.ToString() ?? "";
            var rpm = item?["rpm"]?.GetValue<int>() ?? 0;
            var mode = item?["mode"]?.ToString() ?? "";
            list.Add(new FanModel(id, label, rpm, mode));
        }

        return list;
    }
}
