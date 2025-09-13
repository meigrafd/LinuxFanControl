using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;

namespace LinuxFanControl.Gui.Services
{
    // Minimal localization loader: reads Locales/{code}.json at runtime.
    public static class LocalizationService
    {
        static readonly Dictionary<string, string> _strings = new(StringComparer.OrdinalIgnoreCase);
        public static string CurrentLocale { get; private set; } = "en";

        static string LocalesDir =>
        Path.Combine(AppContext.BaseDirectory, "Locales");

        public static void Load(string localeCode)
        {
            try
            {
                var path = Path.Combine(LocalesDir, $"{localeCode}.json");
                if (!File.Exists(path))
                    throw new FileNotFoundException($"Locale file not found: {path}");

                var json = File.ReadAllText(path);
                var doc = JsonDocument.Parse(json);
                _strings.Clear();
                foreach (var kv in doc.RootElement.EnumerateObject())
                    _strings[kv.Name] = kv.Value.GetString() ?? "";

                CurrentLocale = localeCode;
            }
            catch
            {
                // Keep previous strings; ensure at least English fallback exists
                if (_strings.Count == 0 && !string.Equals(localeCode, "en", StringComparison.OrdinalIgnoreCase))
                {
                    // try fallback en
                    var fallback = Path.Combine(LocalesDir, "en.json");
                    if (File.Exists(fallback))
                    {
                        var json = File.ReadAllText(fallback);
                        var doc = JsonDocument.Parse(json);
                        foreach (var kv in doc.RootElement.EnumerateObject())
                            _strings[kv.Name] = kv.Value.GetString() ?? "";
                        CurrentLocale = "en";
                    }
                }
            }
        }

        public static IEnumerable<string> ListLocales()
        {
            if (!Directory.Exists(LocalesDir))
                yield break;

            foreach (var f in Directory.EnumerateFiles(LocalesDir, "*.json"))
                yield return Path.GetFileNameWithoutExtension(f);
        }

        public static string Get(string key)
        => _strings.TryGetValue(key, out var v) && !string.IsNullOrEmpty(v) ? v : key;
    }
}
