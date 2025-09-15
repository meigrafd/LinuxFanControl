using System.Text.Json;

public static class ConfigSaver
{
    public static void Save(string path, JsonNode data)
    {
        var json = data.ToJsonString(new JsonSerializerOptions
        {
            WriteIndented = true
        });

        File.WriteAllText(path, json);
    }
}
