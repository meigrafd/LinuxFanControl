using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Text.Json;
using Avalonia;
using Avalonia.Themes.Fluent;

namespace LinuxFanControl.Gui.Services
{
    // Runtime theme manager:
    // - Discovers Themes/*.json (e.g. "midnight.json", "light.json")
    // - Each JSON may define { "name": "...", "variant": "dark"|"light" }
    // - No compile-time references; new theme files are picked up automatically.
    public sealed class ThemeManager
    {
        private static readonly Lazy<ThemeManager> _lazy = new(() => new ThemeManager());
        public static ThemeManager Instance => _lazy.Value;

        private readonly List<(string Id, string DisplayName, string Path)> _available = new();
        private string _currentId = "midnight";

        public ReadOnlyCollection<(string Id, string DisplayName, string Path)> AvailableThemes => _available.AsReadOnly();
        public string CurrentId => _currentId;

        private ThemeManager()
        {
            ReloadAvailableThemes();
            // default choose "midnight" when present, else first
            if (!TryApply("midnight") && _available.Count > 0)
                TryApply(_available[0].Id);
        }

        public void ReloadAvailableThemes()
        {
            _available.Clear();
            foreach (var (id, name, path) in DiscoverThemes())
                _available.Add((id, name, path));
        }

        public bool TryApply(string id)
        {
            var entry = _available.Find(x => string.Equals(x.Id, id, StringComparison.OrdinalIgnoreCase));
            if (entry.Path is null) return false;

            try
            {
                using var fs = File.OpenRead(entry.Path);
                using var doc = JsonDocument.Parse(fs);
                var name = doc.RootElement.TryGetProperty("name", out var n) && n.ValueKind == JsonValueKind.String
                ? n.GetString() ?? id
                : id;

                var variant = doc.RootElement.TryGetProperty("variant", out var v) && v.ValueKind == JsonValueKind.String
                ? v.GetString()?.Trim().ToLowerInvariant()
                : "dark";

                var app = Application.Current;
                if (app is null) return false;

                // Replace the base Fluent theme according to variant; colors from JSON can be extended later.
                var fluent = new FluentTheme();
                fluent.Mode = variant == "light" ? FluentThemeMode.Light : FluentThemeMode.Dark;

                app.Styles.Clear();
                app.Styles.Add(fluent);

                _currentId = id;
                return true;
            }
            catch
            {
                return false;
            }
        }

        private static IEnumerable<(string Id, string Name, string Path)> DiscoverThemes()
        {
            foreach (var root in ProbeRoots("Themes"))
            {
                if (!Directory.Exists(root)) continue;
                foreach (var file in Directory.EnumerateFiles(root, "*.json", SearchOption.TopDirectoryOnly))
                {
                    var id = Path.GetFileNameWithoutExtension(file); // e.g. "midnight"
                    var display = id;
                    try
                    {
                        using var fs = File.OpenRead(file);
                        using var doc = JsonDocument.Parse(fs);
                        if (doc.RootElement.TryGetProperty("name", out var nameProp) && nameProp.ValueKind == JsonValueKind.String)
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

            // 2) developer layout
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
