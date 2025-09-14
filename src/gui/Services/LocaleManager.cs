using System.Collections.Generic;
using System.IO;
using System.Text.Json;

namespace FanControl.Gui;

public static class LocaleManager
{
    public static List<string> GetAvailableLocales()
    {
        var locales = new List<string>();
        if (!Directory.Exists("Locales"))
            return locales;

        foreach (var file in Directory.GetFiles("Locales", "*.json"))
        {
            var name = Path.GetFileNameWithoutExtension(file);
            locales.Add(name);
        }

        return locales;
    }

    public static Dictionary<string, string> Load(string localeName)
    {
        string path = Path.Combine("Locales", $"{localeName}.json");
        if (!File.Exists(path))
            return new();

        var json = File.ReadAllText(path);
        return JsonSerializer.Deserialize<Dictionary<string, string>>(json) ?? new();
    }
}
