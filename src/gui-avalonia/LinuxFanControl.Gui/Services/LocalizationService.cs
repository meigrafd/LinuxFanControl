// (c) 2025 LinuxFanControl contributors. MIT License.
// Comments in English, UI texts via external JSON (Locales/*.json).

using System;
using System.IO;
using System.Text.Json;
using System.Collections.Generic;

namespace LinuxFanControl.Gui.Services
{
    /// <summary>
    /// Runtime localization loader + small persistence helpers for GUI config.
    /// - Strings are loaded from Locales/{lang}.json next to the executable.
    /// - GUI config (theme, language) is stored in XDG_CONFIG_HOME (Linux) or
    ///   %AppData% (Windows) under "LinuxFanControl/gui.json".
    /// </summary>
    public sealed class LocalizationService
    {
        public static LocalizationService Instance { get; } = new();

        public string CurrentLanguage { get; private set; } = "en";
        private Dictionary<string, string> _strings = new();

        private LocalizationService()
        {
            // Load default EN at startup to have keys available.
            Load("en");
        }

        /// <summary>Change UI language at runtime (reloads Locales/{lang}.json).</summary>
        public void SetLanguage(string lang) => Load(lang);

        private void Load(string lang)
        {
            var baseDir = AppContext.BaseDirectory;
            var path = Path.Combine(baseDir, "Locales", $"{lang}.json");

            // Fallback to EN if language file is missing
            if (!File.Exists(path))
            {
                lang = "en";
                path = Path.Combine(baseDir, "Locales", "en.json");
            }

            CurrentLanguage = lang;

            try
            {
                var json = File.ReadAllText(path);
                _strings = JsonSerializer.Deserialize<Dictionary<string, string>>(json) ?? new();
            }
            catch
            {
                _strings = new();
            }
        }

        /// <summary>Returns a lightweight accessor for localized strings.</summary>
        public Localizer Localizer => new(_strings);

        // --------------------------------------------------------------------
        // Static helpers expected by ThemeManager.cs
        // --------------------------------------------------------------------

        /// <summary>
        /// Load GUI config (theme + language) from config file, or return defaults.
        /// Returns (themeKey, language).
        /// </summary>
        public static (string themeKey, string language) LoadGuiConfigOrDefault()
        {
            try
            {
                var (cfgPath, _) = GetGuiConfigPath();
                if (File.Exists(cfgPath))
                {
                    using var s = File.OpenRead(cfgPath);
                    var doc = JsonDocument.Parse(s);
                    var root = doc.RootElement;

                    var theme = root.TryGetProperty("theme", out var t) ? t.GetString() ?? "midnight" : "midnight";
                    var lang  = root.TryGetProperty("language", out var l) ? l.GetString() ?? "en"        : "en";

                    // Optionally apply language immediately for convenience:
                    Instance.SetLanguage(lang);

                    return (theme, lang);
                }
            }
            catch
            {
                // ignore and return defaults
            }
            return ("midnight", "en");
        }

        /// <summary>Persist GUI config (theme + language).</summary>
        public static void SaveGuiConfig(string themeKey, string language)
        {
            try
            {
                var (cfgPath, cfgDir) = GetGuiConfigPath();
                Directory.CreateDirectory(cfgDir);

                var payload = new
                {
                    theme = string.IsNullOrWhiteSpace(themeKey) ? "midnight" : themeKey,
                    language = string.IsNullOrWhiteSpace(language) ? "en" : language
                };

                var json = JsonSerializer.Serialize(payload, new JsonSerializerOptions
                {
                    WriteIndented = true
                });
                File.WriteAllText(cfgPath, json);

                // Keep runtime language in sync:
                Instance.SetLanguage(payload.language);
            }
            catch
            {
                // best-effort persistence; swallow IO errors
            }
        }

        /// <summary>
        /// Compute config file path:
        /// Linux:  $XDG_CONFIG_HOME/LinuxFanControl/gui.json or ~/.config/LinuxFanControl/gui.json
        /// Windows: %AppData%/LinuxFanControl/gui.json
        /// macOS:  ~/Library/Application Support/LinuxFanControl/gui.json
        /// </summary>
        private static (string filePath, string dir) GetGuiConfigPath()
        {
            string cfgDir;

            if (OperatingSystem.IsLinux())
            {
                var xdg = Environment.GetEnvironmentVariable("XDG_CONFIG_HOME");
                if (!string.IsNullOrEmpty(xdg))
                    cfgDir = Path.Combine(xdg, "LinuxFanControl");
                else
                    cfgDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".config", "LinuxFanControl");
            }
            else if (OperatingSystem.IsWindows())
            {
                cfgDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "LinuxFanControl");
            }
            else if (OperatingSystem.IsMacOS())
            {
                cfgDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "LinuxFanControl");
            }
            else
            {
                // Generic fallback
                cfgDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "LinuxFanControl");
            }

            var file = Path.Combine(cfgDir, "gui.json");
            return (file, cfgDir);
        }
    }

    /// <summary>Value-type accessor for translated strings.</summary>
    public readonly struct Localizer
    {
        private readonly IReadOnlyDictionary<string, string> _map;
        public Localizer(IReadOnlyDictionary<string, string> map) { _map = map; }

        public string this[string key] => _map != null && _map.TryGetValue(key, out var v) ? v : key;

        // Common strongly-typed accessors used in views:
        public string SetupTitle      => this["setup.title"];
        public string SetupHeadline   => this["setup.headline"];
        public string Theme           => this["setup.theme"];
        public string Language        => this["setup.language"];
        public string RunDetection    => this["setup.run_detection"];
        public string ImportFanControl=> this["setup.import_fc"];
        public string ImportHint      => this["setup.import_hint"];
        public string Cancel          => this["common.cancel"];
        public string Apply           => this["common.apply"];
    }
}
