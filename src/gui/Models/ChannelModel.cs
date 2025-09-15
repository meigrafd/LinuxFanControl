using System.Text.Json.Nodes;

public class ChannelModel
{
    public string Id { get; }
    public string Label { get; }

    public ChannelModel(string id, string label)
    {
        Id = id;
        Label = label;
    }

    public static List<ChannelModel> FromJson(JsonNode? node)
    {
        var list = new List<ChannelModel>();
        if (node is not JsonArray arr) return list;

        foreach (var item in arr)
        {
            var id = item?["id"]?.ToString() ?? "";
            var label = item?["label"]?.ToString() ?? "";
            list.Add(new ChannelModel(id, label));
        }

        return list;
    }
}
