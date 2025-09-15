using System.Text.Json;
using System.Text.Json.Nodes;

public static class SettingsManager
{
    private static readonly string SettingsPath = Path.Combine(AppContext.BaseDirectory, "config", "lfc.json");

    private static readonly Dictionary<string, string> Defaults = new()
    {
        ["language"] = "auto",
        ["theme"] = "default",
        ["lastProfile"] = ""
    };

    private static JsonObject _settings = new();

    public static string Get(string key)
    {
        return _settings[key]?.ToString() ?? Defaults.GetValueOrDefault(key, "");
    }

    public static void Set(string key, string value)
    {
        _settings[key] = value;
        Save();
    }

    public static void Load()
    {
        if (!File.Exists(SettingsPath))
        {
            _settings = new JsonObject(Defaults);
            Save();
            return;
        }

        var json = File.ReadAllText(SettingsPath);
        var node = JsonNode.Parse(json);

        _settings = node?.AsObject() ?? new JsonObject(Defaults);
    }

    public static void Save()
    {
        var json = _settings.ToJsonString(new JsonSerializerOptions
        {
            WriteIndented = true
        });

        Directory.CreateDirectory(Path.GetDirectoryName(SettingsPath)!);
        File.WriteAllText(SettingsPath, json);
    }

    public static IReadOnlyDictionary<string, string> All =>
    _settings.ToDictionary(kv => kv.Key, kv => kv.Value?.ToString() ?? "");
}
