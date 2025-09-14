#nullable enable
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;

namespace LinuxFanControl.Gui.Services
{
    // Minimal runtime i18n loader; strings stored in Locales/{lang}.json
    public static class LocalizationService
    {
        private static readonly Dictionary<string, string> _strings = new();
        private static string _current = "en";

        public static string CurrentLanguage => _current;

        public static string[] ListLanguages()
        {
            var dir = Path.Combine(AppContext.BaseDirectory, "Locales");
            if (!Directory.Exists(dir)) return new[] { "en" };
            var langs = new List<string>();
            foreach (var f in Directory.GetFiles(dir, "*.json"))
            {
                langs.Add(Path.GetFileNameWithoutExtension(f));
            }
            langs.Sort();
            return langs.Count > 0 ? langs.ToArray() : new[] { "en" };
        }

        public static void SetLocale(string language)
        {
            var dir = Path.Combine(AppContext.BaseDirectory, "Locales");
            var path = Path.Combine(dir, $"{language}.json");
            if (!File.Exists(path))
            {
                language = "en";
                path = Path.Combine(dir, "en.json");
            }
            _strings.Clear();
            try
            {
                var json = File.ReadAllText(path);
                var doc = JsonSerializer.Deserialize<Dictionary<string, string>>(json);
                if (doc != null)
                {
                    foreach (var kv in doc) _strings[kv.Key] = kv.Value;
                    _current = language;
                }
            }
            catch
            {
                // fallback
                _strings.Clear();
                _strings["app.title"] = "Linux Fan Control";
            }
        }

        public static string T(string key, string? fallback = null)
            => _strings.TryGetValue(key, out var val) ? val : (fallback ?? key);
    }
}
