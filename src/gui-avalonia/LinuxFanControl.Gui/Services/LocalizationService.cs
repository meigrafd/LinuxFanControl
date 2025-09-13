using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace LinuxFanControl.Gui.Services
{
    /// <summary>
    /// Loads simple key→value JSON from ./Locales (runtime) or repo-local (dev).
    /// File name is the language code: en.json, de.json, …
    /// Example:
    /// { "app.title": "Linux Fan Control", "setup.title": "Setup" }
    /// </summary>
    public static class LocalizationService
    {
        public record LanguageItem(string Code, string Display);

        private static Dictionary<string, string> _strings = new(StringComparer.OrdinalIgnoreCase);
        public static string CurrentLanguage { get; private set; } = "en";

        private static string ResolveDir(string name)
        {
            var baseDir = AppContext.BaseDirectory;
            string[] candidates = new[]
            {
                Path.Combine(baseDir, name),
                Path.GetFullPath(Path.Combine(baseDir, "..", "..", "..", "..", name)),
                Path.GetFullPath(Path.Combine(baseDir, "..", "..", name)),
            };
            return candidates.FirstOrDefault(Directory.Exists) ?? Path.Combine(baseDir, name);
        }

        public static string LocalesDir => ResolveDir("Locales");

        public static LanguageItem[] ListLanguages()
        {
            try
            {
                if (!Directory.Exists(LocalesDir))
                    return new[] { new LanguageItem("en", "English") };

                return Directory.EnumerateFiles(LocalesDir, "*.json")
                .Select(p => Path.GetFileNameWithoutExtension(p))
                .Where(code => !string.IsNullOrWhiteSpace(code))
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .OrderBy(code => code, StringComparer.OrdinalIgnoreCase)
                .Select(code => new LanguageItem(code, CodeToDisplay(code)))
                .ToArray();
            }
            catch
            {
                return new[] { new LanguageItem("en", "English") };
            }
        }

        public static bool SetLanguage(string code)
        {
            try
            {
                var path = Path.Combine(LocalesDir, $"{code}.json");
                if (!File.Exists(path))
                    path = Path.Combine(LocalesDir, "en.json");

                var json = File.Exists(path) ? File.ReadAllText(path) : "{}";
                var map = JsonSerializer.Deserialize<Dictionary<string, string>>(json,
                                                                                 new JsonSerializerOptions { PropertyNameCaseInsensitive = true })
                ?? new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                _strings = new Dictionary<string, string>(map, StringComparer.OrdinalIgnoreCase);
                CurrentLanguage = code;
                return true;
            }
            catch
            {
                _strings = new(StringComparer.OrdinalIgnoreCase);
                CurrentLanguage = "en";
                return false;
            }
        }

        public static string T(string key, string? fallback = null)
        => _strings.TryGetValue(key, out var v) ? v : (fallback ?? key);

        private static string CodeToDisplay(string code) => code.ToLowerInvariant() switch
        {
            "en" => "English",
            "de" => "Deutsch",
            _    => code
        };
    }
}
