// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using Avalonia;
using Avalonia.Media;

namespace LinuxFanControl.Gui.Services
{
    public sealed class ThemeManager
    {
        public sealed record ThemeInfo(string Id, string Name, bool IsDark, IReadOnlyDictionary<string, string> Palette);

        private static readonly Lazy<ThemeManager> _lazy = new(() => new ThemeManager());
        public static ThemeManager Instance => _lazy.Value;

        private readonly List<ThemeInfo> _themes = new();
        private ThemeInfo? _current;

        public IReadOnlyList<ThemeInfo> Themes => _themes;
        public ThemeInfo? Current => _current;

        public static string ThemesRoot => Path.Combine(AppContext.BaseDirectory, "Themes");

        // Resource keys you can bind in XAML: {DynamicResource LFC.BackgroundBrush}, etc.
        private static readonly string[] KnownKeys = new[]
        {
            "Background","Panel","Card","Accent","AccentForeground","Text","SubtleText","Border","GraphLine","GraphFill"
        };

        private ThemeManager()
        {
            Discover();
            var (_, savedTheme) = LocalizationService.LoadGuiConfigOrDefault();
            var pick = _themes.FirstOrDefault(t => t.Id == savedTheme) ?? _themes.FirstOrDefault();
            if (pick is not null) Apply(pick);
        }

        public void Discover()
        {
            _themes.Clear();
            if (!Directory.Exists(ThemesRoot)) return;

            foreach (var file in Directory.EnumerateFiles(ThemesRoot, "*.json", SearchOption.TopDirectoryOnly))
            {
                try
                {
                    using var fs = File.OpenRead(file);
                    using var doc = JsonDocument.Parse(fs);
                    var root = doc.RootElement;

                    var id = Path.GetFileNameWithoutExtension(file) ?? "theme";
                    var name = root.TryGetProperty("name", out var n) && n.ValueKind == JsonValueKind.String
                    ? n.GetString() ?? id : id;
                    var isDark = root.TryGetProperty("dark", out var d) && (d.ValueKind is JsonValueKind.True or JsonValueKind.False)
                    ? d.GetBoolean() : false;

                    var palette = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                    if (root.TryGetProperty("palette", out var pal) && pal.ValueKind == JsonValueKind.Object)
                    {
                        foreach (var kv in pal.EnumerateObject())
                            if (kv.Value.ValueKind == JsonValueKind.String)
                                palette[kv.Name] = kv.Value.GetString() ?? "";
                    }

                    _themes.Add(new ThemeInfo(id, name, isDark, palette));
                }
                catch { /* ignore malformed theme */ }
            }

            _themes.Sort((a, b) => string.Compare(a.Name, b.Name, StringComparison.OrdinalIgnoreCase));
        }

        public void Apply(ThemeInfo theme)
        {
            var app = Application.Current ?? throw new InvalidOperationException("No Avalonia Application");
            var res = app.Resources;

            // Set basic variant (optional)
            app.RequestedThemeVariant = theme.IsDark ? Avalonia.Styling.ThemeVariant.Dark : Avalonia.Styling.ThemeVariant.Light;

            // Apply palette as dynamic brushes/colors
            foreach (var key in KnownKeys)
            {
                var hex = theme.Palette.TryGetValue(key, out var v) ? v : null;
                if (!string.IsNullOrWhiteSpace(hex) && Color.TryParse(hex, out var color))
                {
                    res[$"LFC.{key}Color"] = color;
                    res[$"LFC.{key}Brush"] = new SolidColorBrush(color);
                }
                else
                {
                    // Remove if missing to fallback on defaults in XAML
                    res.Remove($"LFC.{key}Color");
                    res.Remove($"LFC.{key}Brush");
                }
            }

            _current = theme;
            LocalizationService.SaveGuiConfig(language: null, themeId: theme.Id);
        }
    }
}
