// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text.Json;

namespace LinuxFanControl.Gui.Services
{
    public sealed class LocalizationService
    {
        public sealed record LanguageInfo(string Code, string Name);

        private static readonly Lazy<LocalizationService> _lazy = new(() => new LocalizationService());
        public static LocalizationService Instance => _lazy.Value;

        private readonly ConcurrentDictionary<string, IReadOnlyDictionary<string, string>> _cache = new();
        private IReadOnlyDictionary<string, string> _current = new Dictionary<string, string>();
        private string _currentCode = "en";

        public string CurrentCode => _currentCode;
        public event EventHandler? LanguageChanged;

        public static string LocalesRoot => Path.Combine(AppContext.BaseDirectory, "Locales");

        public static IReadOnlyList<LanguageInfo> Discover()
        {
            if (!Directory.Exists(LocalesRoot)) return Array.Empty<LanguageInfo>();
            var list = new List<LanguageInfo>();
            foreach (var path in Directory.EnumerateFiles(LocalesRoot, "*.json", SearchOption.TopDirectoryOnly))
            {
                var code = Path.GetFileNameWithoutExtension(path) ?? "";
                if (string.IsNullOrWhiteSpace(code)) continue;

                try
                {
                    using var fs = File.OpenRead(path);
                    using var doc = JsonDocument.Parse(fs);
                    var name = code;
                    if (doc.RootElement.TryGetProperty("meta", out var meta)
                        && meta.ValueKind == JsonValueKind.Object
                        && meta.TryGetProperty("name", out var nm)
                        && nm.ValueKind == JsonValueKind.String)
                    {
                        name = nm.GetString() ?? code;
                    }
                    list.Add(new LanguageInfo(code, name));
                }
                catch { /* ignore */ }
            }
            return list.OrderBy(l => l.Name, StringComparer.OrdinalIgnoreCase).ToArray();
        }

        public void Load(string code)
        {
            _current = LoadMap(code);
            _currentCode = code;
            SaveGuiConfig(language: code, themeId: null);
            LanguageChanged?.Invoke(this, EventArgs.Empty);
        }

        public string T(string key)
        {
            if (_current.TryGetValue(key, out var v)) return v;
            if (_currentCode != "en" && LoadMap("en").TryGetValue(key, out var v2)) return v2;
            return key;
        }

        private IReadOnlyDictionary<string, string> LoadMap(string code)
        {
            return _cache.GetOrAdd(code, static c =>
            {
                var file = Path.Combine(LocalesRoot, $"{c}.json");
                if (!File.Exists(file)) return new Dictionary<string, string>();
                try
                {
                    using var fs = File.OpenRead(file);
                    using var doc = JsonDocument.Parse(fs);
                    var dict = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                    foreach (var prop in doc.RootElement.EnumerateObject())
                    {
                        if (prop.NameEquals("meta")) continue;
                        if (prop.Value.ValueKind == JsonValueKind.String)
                            dict[prop.Name] = prop.Value.GetString() ?? "";
                    }
                    return dict;
                }
                catch { return new Dictionary<string, string>(); }
            });
        }

        // ---- persistence (shared with theme) ----
        private static string ConfigDir()
        {
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
                return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "LinuxFanControl");
            var xdg = Environment.GetEnvironmentVariable("XDG_CONFIG_HOME");
            if (!string.IsNullOrEmpty(xdg)) return Path.Combine(xdg, "LinuxFanControl");
            return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".config", "LinuxFanControl");
        }
        private static string ConfigPath() => Path.Combine(ConfigDir(), "gui.json");

        public static (string language, string? theme) LoadGuiConfigOrDefault()
        {
            try
            {
                var p = ConfigPath();
                if (!File.Exists(p)) return ("en", null);
                using var fs = File.OpenRead(p);
                using var doc = JsonDocument.Parse(fs);
                string lang = "en";
                string? theme = null;
                if (doc.RootElement.TryGetProperty("language", out var l) && l.ValueKind == JsonValueKind.String)
                    lang = l.GetString() ?? "en";
                if (doc.RootElement.TryGetProperty("theme", out var t) && t.ValueKind == JsonValueKind.String)
                    theme = t.GetString();
                return (string.IsNullOrWhiteSpace(lang) ? "en" : lang, theme);
            }
            catch { return ("en", null); }
        }

        public static void SaveGuiConfig(string? language, string? themeId)
        {
            try
            {
                var (curLang, curTheme) = LoadGuiConfigOrDefault();
                var lang = language ?? curLang;
                var theme = themeId ?? curTheme;
                Directory.CreateDirectory(ConfigDir());
                var obj = new Dictionary<string, object?> { ["language"] = lang, ["theme"] = theme };
                var json = JsonSerializer.Serialize(obj, new JsonSerializerOptions { WriteIndented = true });
                File.WriteAllText(ConfigPath(), json);
            }
            catch { }
        }
    }
}
