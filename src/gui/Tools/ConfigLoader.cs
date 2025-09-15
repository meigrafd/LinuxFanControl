using System.Text.Json;

public static class ConfigLoader
{
    public static JsonNode? Load(string path)
    {
        if (!File.Exists(path))
            return null;

        var json = File.ReadAllText(path);
        return JsonNode.Parse(json);
    }
}
