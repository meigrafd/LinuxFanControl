using System.Globalization;
using System.Text.Json.Nodes;

public static class LocaleManager
{
    private static Dictionary<string, string> _dict = new();

    public static void Load(string langCode)
    {
        if (langCode == "auto")
            langCode = CultureInfo.CurrentCulture.TwoLetterISOLanguageName;

        var path = Path.Combine(AppContext.BaseDirectory, "Locales", $"{langCode}.json");

        if (!File.Exists(path))
            throw new FileNotFoundException($"Locale file not found: {path}");

        var json = File.ReadAllText(path);
        var node = JsonNode.Parse(json);

        _dict.Clear();
        foreach (var kv in node?.AsObject() ?? [])
            _dict[kv.Key] = kv.Value?.ToString() ?? kv.Key;
    }

    public static string _(string key)
    {
        return _dict.TryGetValue(key, out var value) ? value : $"[{key}]";
    }
}
