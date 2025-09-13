using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Globalization;
using System.IO;
using System.Text.Json;

namespace LinuxFanControl.Gui.Services
{
    // Simple runtime localization service:
    // - Discovers all *.json files under Locales/
    // - Each file is a flat key->string map: { "menu.setup": "Setup", ... }
    // - No hardcoded languages; new files appear automatically without rebuild.
    public sealed class LocalizationService
    {
        private static readonly Lazy<LocalizationService> _lazy = new(() => new LocalizationService());
        public static LocalizationService Instance => _lazy.Value;

        private readonly Dictionary<string, string> _strings = new(StringComparer.OrdinalIgnoreCase);
        private readonly List<(string Id, string DisplayName, string Path)> _available = new();
        private string _currentId = "en";

        public ReadOnlyCollection<(string Id, string DisplayName, string Path)> AvailableLocales => _available.AsReadOnly();
        public string CurrentId => _currentId;

        private LocalizationService()
        {
            ReloadAvailableLocales();
            // Pick default by system culture if available, else "en"
            var sys = CultureInfo.CurrentUICulture.TwoLetterISOLanguageName;
            if (!TrySetLocale(sys)) TrySetLocale("en");
        }

        public void ReloadAvailableLocales()
        {
            _available.Clear();
            foreach (var (id, name, path) in DiscoverLocales())
                _available.Add((id, name, path));
        }

        public bool TrySetLocale(string id)
        {
            var entry = _available.Find(x => string.Equals(x.Id, id, StringComparison.OrdinalIgnoreCase));
            if (entry.Path is null) return false;
            try
            {
                var txt = File.ReadAllText(entry.Path);
                var doc = JsonSerializer.Deserialize<Dictionary<string, string>>(txt) ?? new();
                _strings.Clear();
                foreach (var kv in doc)
                    _strings[kv.Key] = kv.Value ?? string.Empty;
                _currentId = entry.Id;
                return true;
            }
            catch
            {
                return false;
            }
        }

        public string T(string key, string? fallback = null)
        => _strings.TryGetValue(key, out var v) ? v : (fallback ?? key);

        private static IEnumerable<(string Id, string Name, string Path)> DiscoverLocales()
        {
            foreach (var root in ProbeRoots("Locales"))
            {
                if (!Directory.Exists(root)) continue;
                foreach (var file in Directory.EnumerateFiles(root, "*.json", SearchOption.TopDirectoryOnly))
                {
                    var id = Path.GetFileNameWithoutExtension(file); // e.g. "en", "de"
                    var display = id;
                    try
                    {
                        using var fs = File.OpenRead(file);
                        using var doc = JsonDocument.Parse(fs);
                        if (doc.RootElement.TryGetProperty("__name", out var nameProp) && nameProp.ValueKind == JsonValueKind.String)
                            display = nameProp.GetString() ?? id;
                    }
                    catch { /* ignore */ }
                    yield return (id, display, file);
                }
            }
        }

        private static IEnumerable<string> ProbeRoots(string leafDir)
        {
            // 1) alongside executable (publish/run)
            var baseDir = AppContext.BaseDirectory;
            yield return Path.Combine(baseDir, leafDir);

            // 2) developer layout: project root (when running from bin/Debug|Release)
            // Walk up a few levels looking for the project dir that contains this leaf
            var d = new DirectoryInfo(baseDir);
            for (int i = 0; i < 6 && d != null; i++, d = d.Parent)
            {
                var cand = Path.Combine(d.FullName, "src", "gui-avalonia", "LinuxFanControl.Gui", leafDir);
                if (Directory.Exists(cand)) { yield return cand; break; }
            }

            // 3) optional override via env
            var overrideRoot = Environment.GetEnvironmentVariable("LFC_GUI_ASSETS_ROOT");
            if (!string.IsNullOrWhiteSpace(overrideRoot))
                yield return Path.Combine(overrideRoot, leafDir);
        }
    }
}
