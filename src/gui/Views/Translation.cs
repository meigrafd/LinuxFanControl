using System.Collections.Generic;
using System.IO;
using System.Text.Json;

namespace FanControl.Gui;

public static class Translation
{
    private static Dictionary<string, string> _strings = new();
    private static string _currentLocale = "en";

    public static void Load(string locale)
    {
        var path = Path.Combine("Locales", $"{locale}.json");
        if (!File.Exists(path))
            return;

        var json = File.ReadAllText(path);
        _strings = JsonSerializer.Deserialize<Dictionary<string, string>>(json) ?? new();
        _currentLocale = locale;
    }

    public static string T(string key)
    {
        if (_strings.TryGetValue(key, out var value))
            return value;

        return $"[{key}]";
    }

    public static string CurrentLocale => _currentLocale;
}
