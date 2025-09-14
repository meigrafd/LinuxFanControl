using System;
using System.IO;
using System.Linq;
using System.Text.Json;
using Avalonia;
using Avalonia.Media;

namespace LinuxFanControl.Gui.Services
{
    // Runtime theme loader: Themes/{name}.json => maps to resource keys
    // Keys: Lfc.WindowBg, Lfc.CardBg, Lfc.TextPrimary, Lfc.TextSecondary, Lfc.Accent, Variant(light|dark)
    public static class ThemeManager
    {
        public static string[] ListThemes()
        {
            var dir = Path.Combine(AppContext.BaseDirectory, "Themes");
            if (!Directory.Exists(dir)) return new[] { "midnight", "light" };
            var list = Directory.GetFiles(dir, "*.json").Select(Path.GetFileNameWithoutExtension).OrderBy(s => s).ToArray();
            return list.Length > 0 ? list : new[] { "midnight", "light" };
        }

        public static void ApplyTheme(string name)
        {
            var dir = Path.Combine(AppContext.BaseDirectory, "Themes");
            var path = Path.Combine(dir, $"{name}.json");
            if (!File.Exists(path))
            {
                name = "midnight";
                path = Path.Combine(dir, "midnight.json");
            }
            try
            {
                var json = File.ReadAllText(path);
                var doc = JsonDocument.Parse(json).RootElement;
                var pal = doc.GetProperty("Palette");
                string Get(string k, string def) => pal.TryGetProperty(k, out var v) ? v.GetString() ?? def : def;

                var app = Application.Current!;
                app.Resources["Lfc.WindowBg"] = new SolidColorBrush(Color.Parse(Get("WindowBackground", "#111827")));
                app.Resources["Lfc.CardBg"] = new SolidColorBrush(Color.Parse(Get("CardBackground", "#1f2937")));
                app.Resources["Lfc.TextPrimary"] = new SolidColorBrush(Color.Parse(Get("TextPrimary", "#e5e7eb")));
                app.Resources["Lfc.TextSecondary"] = new SolidColorBrush(Color.Parse(Get("TextSecondary", "#9ca3af")));
                app.Resources["Lfc.Accent"] = new SolidColorBrush(Color.Parse(Get("Accent", "#2563eb")));

                var variant = doc.TryGetProperty("Variant", out var vElem) ? vElem.GetString() : "dark";
                app.RequestedThemeVariant = (variant?.ToLowerInvariant() == "light") ? Avalonia.Styling.ThemeVariant.Light : Avalonia.Styling.ThemeVariant.Dark;
            }
            catch
            {
                // ignore and keep defaults
            }
        }
    }
}
