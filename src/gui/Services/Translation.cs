using System.Collections.Generic;
using System.IO;
using System.Text.Json;

namespace FanControl.Gui.Views;

public static class Translation
{
    private static Dictionary<string, string> _entries = new();
    private static string _currentLocale = "en";

    public static event Action? LanguageChanged;

    public static void Load(string locale)
    {
        _currentLocale = locale;

        string path = Path.Combine("locales", $"{locale}.json");
        if (!File.Exists(path))
        {
            _entries = new();
        }
        else
        {
            string json = File.ReadAllText(path);
            _entries = JsonSerializer.Deserialize<Dictionary<string, string>>(json)
            ?? new Dictionary<string, string>();
        }

        LanguageChanged?.Invoke();
    }

    public static string Get(string key)
    {
        if (_entries.TryGetValue(key, out var value))
            return value;

        return $"[{key}]";
    }

    public static string CurrentLocale => _currentLocale;

    public static string[] AvailableLocales()
    {
        string dir = Path.Combine("locales");
        if (!Directory.Exists(dir))
            return new[] { "en" };

        return Directory.GetFiles(dir, "*.json")
        .Select(f => Path.GetFileNameWithoutExtension(f))
        .ToArray();
    }
}
