using System;
using System.IO;
using System.Text.Json;
using System.Collections.Generic;
using System.Linq;

namespace LinuxFanControl.Gui.Services
{
    /// <summary>
    /// Loads ./Locales/*.json and provides string lookup by key.
    /// Nothing is hardcoded; language files can be added without recompiling.
    /// </summary>
    public sealed class LocalizationService
    {
        public static LocalizationService Instance { get; } = new();

        public static string LocalesDir =>
        Path.Combine(AppContext.BaseDirectory, "Locales");

        public event Action? LanguageChanged;

        public string CurrentLanguage { get; private set; } = "en";
        Dictionary<string, string> _table = new(StringComparer.OrdinalIgnoreCase);

        LocalizationService()
        {
            var langs = ListLanguages();
            CurrentLanguage = langs.Contains("en") ? "en" : langs.FirstOrDefault() ?? "en";
            Load(CurrentLanguage);
        }

        public IReadOnlyList<string> ListLanguages()
        {
            if (!Directory.Exists(LocalesDir)) return Array.Empty<string>();
            return Directory.GetFiles(LocalesDir, "*.json")
            .Select(Path.GetFileNameWithoutExtension)
            .OrderBy(n => n, StringComparer.OrdinalIgnoreCase)
            .ToList();
        }

        public void Load(string lang)
        {
            var f = Path.Combine(LocalesDir, $"{lang}.json");
            if (!File.Exists(f))
                throw new FileNotFoundException($"Locale file not found: {f}");

            var json = File.ReadAllText(f);
            var dict = JsonSerializer.Deserialize<Dictionary<string, string>>(json)
            ?? new Dictionary<string, string>();

            _table = new Dictionary<string, string>(dict, StringComparer.OrdinalIgnoreCase);
            CurrentLanguage = lang;
            LanguageChanged?.Invoke();
        }

        public string T(string key, string? fallback = null)
        => _table.TryGetValue(key, out var v) ? v : (fallback ?? key);
    }
}
