using System.Text.Json.Nodes;

public class SensorModel
{
    public string Id { get; }
    public string Label { get; }
    public float Value { get; }
    public string Source { get; }

    public SensorModel(string id, string label, float value, string source)
    {
        Id = id;
        Label = label;
        Value = value;
        Source = source;
    }

    public static List<SensorModel> FromJson(JsonNode? node)
    {
        var list = new List<SensorModel>();
        if (node is not JsonArray arr) return list;

        foreach (var item in arr)
        {
            var id = item?["id"]?.ToString() ?? "";
            var label = item?["label"]?.ToString() ?? "";
            var value = item?["value"]?.GetValue<float>() ?? 0f;
            var source = item?["source"]?.ToString() ?? "";
            list.Add(new SensorModel(id, label, value, source));
        }

        return list;
    }
}
