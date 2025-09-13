using System;
using System.IO;
using System.Linq;
using System.Text.Json;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Markup.Xaml.MarkupExtensions;
using Avalonia.Media;
using Avalonia.Styling;

namespace LinuxFanControl.Gui.Services
{
    /// <summary>
    /// Load & apply simple JSON themes from ./Themes. No hardcoded csproj includes required.
    /// </summary>
    public static class ThemeManager
    {
        public const string DefaultThemeName = "midnight";
        public static string DefaultTheme => DefaultThemeName;
        public static string CurrentTheme { get; private set; } = DefaultThemeName;

        public sealed class ThemeColors
        {
            public string background { get; set; } = "#0E1A2B";
            public string panel      { get; set; } = "#14233A";
            public string panelAlt   { get; set; } = "#0B1626";
            public string accent     { get; set; } = "#3B82F6";
            public string accent2    { get; set; } = "#60A5FA";
            public string text       { get; set; } = "#E6EDF3";
            public string textMuted  { get; set; } = "#93A4B3";
        }
        public sealed class ThemeSpec
        {
            public string name { get; set; } = DefaultThemeName;
            public ThemeColors colors { get; set; } = new ThemeColors();
        }

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

        public static string ThemesDir => ResolveDir("Themes");

        public static string[] ListThemes()
        {
            try
            {
                if (!Directory.Exists(ThemesDir)) return new[] { DefaultThemeName };
                return Directory.EnumerateFiles(ThemesDir, "*.json")
                .Select(p => Path.GetFileNameWithoutExtension(p))
                .Where(n => !string.IsNullOrWhiteSpace(n))
                .Select(n => n!) // ensure non-null for nullable flow analysis
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .OrderBy(n => n, StringComparer.OrdinalIgnoreCase)
                .ToArray();
            }
            catch
            {
                return new[] { DefaultThemeName };
            }
        }

        public static bool ApplyTheme(string themeName)
        {
            try
            {
                var path = Path.Combine(ThemesDir, $"{themeName}.json");
                ThemeSpec spec;
                if (File.Exists(path))
                {
                    var json = File.ReadAllText(path);
                    spec = JsonSerializer.Deserialize<ThemeSpec>(json, new JsonSerializerOptions
                    {
                        PropertyNameCaseInsensitive = true
                    }) ?? new ThemeSpec();
                }
                else
                {
                    spec = new ThemeSpec();
                }
                Apply(spec);
                CurrentTheme = string.IsNullOrWhiteSpace(spec.name) ? themeName : spec.name;
                return true;
            }
            catch
            {
                Apply(new ThemeSpec());
                CurrentTheme = DefaultThemeName;
                return false;
            }
        }

        private static void Apply(ThemeSpec spec)
        {
            if (Application.Current is null) return;
            var res = Application.Current.Resources;

            var bBg      = new SolidColorBrush(Parse(spec.colors.background));
            var bPanel   = new SolidColorBrush(Parse(spec.colors.panel));
            var bPanel2  = new SolidColorBrush(Parse(spec.colors.panelAlt));
            var bAccent  = new SolidColorBrush(Parse(spec.colors.accent));
            var bAccent2 = new SolidColorBrush(Parse(spec.colors.accent2));
            var bText    = new SolidColorBrush(Parse(spec.colors.text));
            var bText2   = new SolidColorBrush(Parse(spec.colors.textMuted));

            res["ThemeBackgroundBrush"]  = bBg;
            res["SystemAccentColor"]     = bAccent.Color;
            res["SystemAccentBrush"]     = bAccent;
            res["SystemAccentBrush2"]    = bAccent2;

            res["Lfc/Background"]        = bBg;
            res["Lfc/Panel"]             = bPanel;
            res["Lfc/PanelAlt"]          = bPanel2;
            res["Lfc/Accent"]            = bAccent;
            res["Lfc/Accent2"]           = bAccent2;
            res["Lfc/Text"]              = bText;
            res["Lfc/TextMuted"]         = bText2;

            res["ControlBackground"]     = bPanel;
            res["ControlForeground"]     = bText;
            res["ButtonBackground"]      = bPanel2;
            res["ButtonForeground"]      = bText;

            EnsureGlobalStyles();
        }

        private static void EnsureGlobalStyles()
        {
            if (Application.Current is null) return;

            foreach (var s in Application.Current.Styles)
            {
                if (s is Style st && st.Selector?.ToString()?.Contains("Window") == true)
                    return;
            }

            var styles = new Styles
            {
                new Style(x => x.OfType<Window>())
                {
                    Setters =
                    {
                        new Setter(Window.BackgroundProperty,
                                   new DynamicResourceExtension("Lfc/Background"))
                    }
                },
                new Style(x => x.OfType<UserControl>())
                {
                    Setters =
                    {
                        new Setter(TemplatedControl.BackgroundProperty,
                                   new DynamicResourceExtension("Lfc/Panel"))
                    }
                },
                new Style(x => x.OfType<TextBlock>())
                {
                    Setters =
                    {
                        new Setter(TextBlock.ForegroundProperty,
                                   new DynamicResourceExtension("Lfc/Text"))
                    }
                },
                new Style(x => x.OfType<Button>())
                {
                    Setters =
                    {
                        new Setter(TemplatedControl.BackgroundProperty,
                                   new DynamicResourceExtension("ButtonBackground")),
                                   new Setter(TemplatedControl.ForegroundProperty,
                                              new DynamicResourceExtension("ButtonForeground"))
                    }
                }
            };

            Application.Current.Styles.Add(styles);
        }

        private static Color Parse(string hex) =>
        Color.TryParse(hex, out var c) ? c : Colors.White;
    }
}
