using System;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Collections.Generic;
using Avalonia;
using Avalonia.Media;

namespace LinuxFanControl.Gui.Services
{
    /// <summary>
    /// Loads simple theme JSON files from ./Themes/*.json and applies as resource keys.
    /// Nothing is hardcoded: all keys in the JSON become resources at Application.Current.Resources[key].
    /// </summary>
    public static class ThemeManager
    {
        public static string ThemesDir =>
        Path.Combine(AppContext.BaseDirectory, "Themes");

        public static IReadOnlyList<string> ListThemes()
        {
            if (!Directory.Exists(ThemesDir)) return Array.Empty<string>();
            return Directory.GetFiles(ThemesDir, "*.json")
            .Select(Path.GetFileNameWithoutExtension)
            .OrderBy(n => n, StringComparer.OrdinalIgnoreCase)
            .ToList();
        }

        public static string DefaultTheme()
        => ListThemes().FirstOrDefault() ?? "midnight";

        public static string CurrentTheme { get; private set; } = "";

        public static void Apply(string themeName)
        {
            var file = Path.Combine(ThemesDir, $"{themeName}.json");
            if (!File.Exists(file))
                throw new FileNotFoundException($"Theme file not found: {file}");

            var json = File.ReadAllText(file);
            var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;

            var app = Application.Current ?? throw new InvalidOperationException("No Avalonia Application running.");
            var res = app.Resources;

            // Clear only keys that were previously set by ThemeManager (we track by prefix).
            var toRemove = res.Keys.OfType<object>()
            .Where(k => k is string s && s.StartsWith("theme:", StringComparison.Ordinal))
            .ToList();
            foreach (var k in toRemove) res.Remove(k);

            // Flatten JSON to resources: theme:<path> -> value (Brush/Color/string/double)
            void put(string key, JsonElement el)
            {
                string resKey = $"theme:{key}";
                switch (el.ValueKind)
                {
                    case JsonValueKind.Number:
                        if (el.TryGetDouble(out var d)) res[resKey] = d;
                        break;
                    case JsonValueKind.String:
                        var s = el.GetString() ?? "";
                        // try color => SolidColorBrush
                        if (TryParseColor(s, out var color))
                            res[resKey] = new SolidColorBrush(color);
                    else
                        res[resKey] = s;
                    break;
                    case JsonValueKind.True:
                    case JsonValueKind.False:
                        res[resKey] = el.GetBoolean();
                        break;
                }
            }

            void walk(JsonElement e, string prefix)
            {
                if (e.ValueKind == JsonValueKind.Object)
                {
                    foreach (var p in e.EnumerateObject())
                        walk(p.Value, string.IsNullOrEmpty(prefix) ? p.Name : $"{prefix}.{p.Name}");
                }
                else
                {
                    put(prefix, e);
                }
            }

            walk(root, "");

            CurrentTheme = themeName;
        }

        static bool TryParseColor(string s, out Color color)
        {
            s = s.Trim();
            if (s.StartsWith("#", StringComparison.Ordinal))
            {
                try
                {
                    color = Color.Parse(s);
                    return true;
                }
                catch { }
            }
            color = default;
            return false;
        }
    }
}
