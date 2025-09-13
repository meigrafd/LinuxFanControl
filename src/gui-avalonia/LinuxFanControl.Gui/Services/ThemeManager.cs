using System;
using System.IO;
using System.Text.Json;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Styling;

namespace LinuxFanControl.Gui.Services
{
    /// <summary>
    /// Loads a simple JSON theme and applies brushes/colors to Application.Resources.
    /// No hard-coded theme names in project files; themes are plain JSON next to the executable.
    /// </summary>
    public static class ThemeManager
    {
        public const string DefaultThemeName = "midnight";

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

        public static string ThemesDir =>
        Path.Combine(AppContext.BaseDirectory, "Themes");

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
                    spec = new ThemeSpec(); // fallback midnight palette
                }

                Apply(spec);
                return true;
            }
            catch
            {
                // Very last resort: fallback palette
                Apply(new ThemeSpec());
                return false;
            }
        }

        private static void Apply(ThemeSpec spec)
        {
            if (Application.Current is null) return;

            var res = Application.Current.Resources;

            // Parse colors
            var bg      = Parse(spec.colors.background);
            var panel   = Parse(spec.colors.panel);
            var panel2  = Parse(spec.colors.panelAlt);
            var accent  = Parse(spec.colors.accent);
            var accent2 = Parse(spec.colors.accent2);
            var text    = Parse(spec.colors.text);
            var text2   = Parse(spec.colors.textMuted);

            // Create brushes
            var bBg      = new SolidColorBrush(bg);
            var bPanel   = new SolidColorBrush(panel);
            var bPanel2  = new SolidColorBrush(panel2);
            var bAccent  = new SolidColorBrush(accent);
            var bAccent2 = new SolidColorBrush(accent2);
            var bText    = new SolidColorBrush(text);
            var bText2   = new SolidColorBrush(text2);

            // Common keys (Fluent-ish + custom)
            res["ThemeBackgroundBrush"]      = bBg;
            res["SystemAccentColor"]         = accent;
            res["SystemAccentBrush"]         = bAccent;
            res["SystemAccentBrush2"]        = bAccent2;

            // Our custom keys used by views/controls
            res["Lfc/Background"]            = bBg;
            res["Lfc/Panel"]                 = bPanel;
            res["Lfc/PanelAlt"]              = bPanel2;
            res["Lfc/Accent"]                = bAccent;
            res["Lfc/Accent2"]               = bAccent2;
            res["Lfc/Text"]                  = bText;
            res["Lfc/TextMuted"]             = bText2;

            // Reasonable defaults for common controls (keeps things readable)
            res["ControlBackground"]         = bPanel;
            res["ControlForeground"]         = bText;
            res["ButtonBackground"]          = bPanel2;
            res["ButtonForeground"]          = bText;
            res["TextControlForeground"]     = bText;

            // Global style to ensure windows/usercontrols pick up the theme background/panel
            EnsureGlobalStyles();
        }

        private static void EnsureGlobalStyles()
        {
            if (Application.Current is null) return;

            // Avoid adding duplicates
            foreach (var s in Application.Current.Styles)
            {
                if (s is Style st && st.Selector?.ToString()?.Contains("Window") == true)
                    return;
            }

            var styles = new Styles
            {
                // Window background = app background
                new Style(x => x.OfType<Window>())
                {
                    Setters =
                    {
                        new Setter(Window.BackgroundProperty,
                                   new DynamicResourceExtension("Lfc/Background"))
                    }
                },
                // UserControl background = panel (makes tiles/cards readable)
                new Style(x => x.OfType<UserControl>())
                {
                    Setters =
                    {
                        new Setter(TemplatedControl.BackgroundProperty,
                                   new DynamicResourceExtension("Lfc/Panel"))
                    }
                },
                // TextBlock foreground = text
                new Style(x => x.OfType<TextBlock>())
                {
                    Setters =
                    {
                        new Setter(TextBlock.ForegroundProperty,
                                   new DynamicResourceExtension("Lfc/Text"))
                    }
                },
                // Buttons use panelAlt/bg + text
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

        private static Color Parse(string hex)
        {
            // Accept #RRGGBB or #AARRGGBB
            if (Color.TryParse(hex, out var c)) return c;
            // fallback white (should never happen with our defaults)
            return Colors.White;
        }
    }
}
