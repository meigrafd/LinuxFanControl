// (c) 2025 LinuxFanControl contributors. MIT License.
// Simple dynamic theme loader from Themes/*.json. Applies colors to Application resources.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using Avalonia;
using Avalonia.Media;

namespace LinuxFanControl.Gui.Services
{
    public sealed class ThemeInfo
    {
        public string Id { get; init; } = "";
        public string Name { get; init; } = "";
        public string Path { get; init; } = "";
        public override string ToString() => Name;
    }

    public static class ThemeService
    {
        // Known resource keys used by the XAML
        private static readonly string[] Keys =
        {
            "LFC.BackgroundBrush",
            "LFC.PanelBrush",
            "LFC.CardBrush",
            "LFC.TextBrush",
            "LFC.SubtleTextBrush",
            "LFC.AccentBrush",
            "LFC.BorderBrush"
        };

        public static IReadOnlyList<ThemeInfo> Enumerate()
        {
            var root = GetThemeRoot();
            if (!Directory.Exists(root)) return Array.Empty<ThemeInfo>();

            var list = new List<ThemeInfo>();
            foreach (var file in Directory.EnumerateFiles(root, "*.json", SearchOption.TopDirectoryOnly))
            {
                var id = System.IO.Path.GetFileNameWithoutExtension(file);
                var name = ToTitleCase(id);
                list.Add(new ThemeInfo { Id = id, Name = name, Path = file });
            }
            return list.OrderBy(x => x.Name).ToArray();
        }

        public static void Apply(ThemeInfo theme)
        {
            if (Application.Current is null) return;
            if (!File.Exists(theme.Path)) return;

            var json = File.ReadAllText(theme.Path);
            var doc = JsonDocument.Parse(json);

            if (doc.RootElement.ValueKind != JsonValueKind.Object) return;

            var dict = new Dictionary<string, IBrush>(StringComparer.Ordinal);
            foreach (var k in Keys)
            {
                if (doc.RootElement.TryGetProperty(k, out var el) && el.ValueKind == JsonValueKind.String)
                {
                    var color = ParseColor(el.GetString() ?? "");
                    dict[k] = new SolidColorBrush(color);
                }
            }

            var res = Application.Current.Resources;
            foreach (var kv in dict)
                res[kv.Key] = kv.Value;
        }

        public static string GetThemeRoot()
        {
            // Relative to executable: ./Themes/*.json
            var baseDir = AppContext.BaseDirectory;
            var folder = System.IO.Path.Combine(baseDir, "Themes");
            return folder;
        }

        private static string ToTitleCase(string id)
        {
            if (string.IsNullOrWhiteSpace(id)) return "Theme";
            return System.Globalization.CultureInfo.InvariantCulture.TextInfo.ToTitleCase(id.Replace('_',' ').Replace('-',' '));
        }

        private static Color ParseColor(string value)
        {
            // Accept #RRGGBB or #AARRGGBB
            try
            {
                if (value.StartsWith('#'))
                {
                    if (value.Length == 7)
                    {
                        var r = byte.Parse(value.Substring(1,2), System.Globalization.NumberStyles.HexNumber);
                        var g = byte.Parse(value.Substring(3,2), System.Globalization.NumberStyles.HexNumber);
                        var b = byte.Parse(value.Substring(5,2), System.Globalization.NumberStyles.HexNumber);
                        return Color.FromArgb(255, r, g, b);
                    }
                    if (value.Length == 9)
                    {
                        var a = byte.Parse(value.Substring(1,2), System.Globalization.NumberStyles.HexNumber);
                        var r = byte.Parse(value.Substring(3,2), System.Globalization.NumberStyles.HexNumber);
                        var g = byte.Parse(value.Substring(5,2), System.Globalization.NumberStyles.HexNumber);
                        var b = byte.Parse(value.Substring(7,2), System.Globalization.NumberStyles.HexNumber);
                        return Color.FromArgb(a, r, g, b);
                    }
                }
            }
            catch { }
            // Fallback: slate-ish
            return Color.Parse("#0F172A");
        }
    }
}
