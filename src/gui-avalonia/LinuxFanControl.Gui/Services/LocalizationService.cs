using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Text.Json;

namespace LinuxFanControl.Gui.Services
{
    /// <summary>
    /// Loads UI strings from external JSON files (Locales/{lang}.json). No hardcoding.
    /// Also persists GUI preferences (language, theme) in ~/.config/LinuxFanControl/gui.json.
    /// </summary>
    public static class LocalizationService
    {
        private static readonly Dictionary<string, string> _strings = new(StringComparer.OrdinalIgnoreCase);

        public static string DefaultLanguage => "en";

        private static string LocalesRoot
        => Environment.GetEnvironmentVariable("LFC_LOCALES_DIR")
        ?? Path.Combine(AppContext.BaseDirectory, "Locales");

        private static string ConfigPath
        => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
                        ".config", "LinuxFanControl", "gui.json");

        public static IReadOnlyList<string> ListLanguages()
        {
            var list = new List<string>();
            if (Directory.Exists(LocalesRoot))
            {
                foreach (var f in Directory.EnumerateFiles(LocalesRoot, "*.json"))
                    list.Add(Path.GetFileNameWithoutExtension(f));
            }
            if (list.Count == 0) list.Add(DefaultLanguage);
            return new ReadOnlyCollection<string>(list);
        }

        public static void SetLanguage(string language)
        {
            var file = Path.Combine(LocalesRoot, $"{language}.json");
            _strings.Clear();
            if (File.Exists(file))
            {
                var json = File.ReadAllText(file);
                var doc = JsonDocument.Parse(json);
                foreach (var kv in doc.RootElement.EnumerateObject())
                    _strings[kv.Name] = kv.Value.GetString() ?? string.Empty;
            }
            else
            {
                // minimal fallback
                _strings["ui.ok"] = "OK";
                _strings["ui.cancel"] = "Cancel";
            }
        }

        public static string GetString(string key)
        => _strings.TryGetValue(key, out var value) ? value : key;

        public static (string language, string theme) LoadGuiConfigOrDefault()
        {
            try
            {
                if (File.Exists(ConfigPath))
                {
                    var json = File.ReadAllText(ConfigPath);
                    var doc = JsonDocument.Parse(json);
                    var root = doc.RootElement;
                    var lang = root.TryGetProperty("language", out var l) ? l.GetString() ?? DefaultLanguage : DefaultLanguage;
                    var theme = root.TryGetProperty("theme", out var t) ? t.GetString() ?? ThemeManager.DefaultTheme : ThemeManager.DefaultTheme;
                    SetLanguage(lang);
                    return (lang, theme);
                }
            }
            catch { /* ignore */ }

            SetLanguage(DefaultLanguage);
            return (DefaultLanguage, ThemeManager.DefaultTheme);
        }

        public static void SaveGuiConfig(string language, string theme)
        {
            try
            {
                var dir = Path.GetDirectoryName(ConfigPath)!;
                Directory.CreateDirectory(dir);
                var payload = new { language, theme };
                var json = JsonSerializer.Serialize(payload, new JsonSerializerOptions { WriteIndented = true });
                File.WriteAllText(ConfigPath, json);
            }
            catch { /* ignore */ }
        }
    }
}
