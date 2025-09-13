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
    public sealed class ThemeInfo
    {
        public string Id { get; init; } = "";
        public string Name { get; init; } = "";
        public string Path { get; init; } = "";
        public override string ToString() => Name;
    }

    public static class ThemeService
    {
        private static readonly string[] Keys =
        {
            "LFC.BackgroundBrush","LFC.PanelBrush","LFC.CardBrush","LFC.TextBrush",
            "LFC.SubtleTextBrush","LFC.AccentBrush","LFC.BorderBrush"
        };

        public static IReadOnlyList<ThemeInfo> Enumerate()
        {
            var root = Path.Combine(AppContext.BaseDirectory, "Themes");
            if (!Directory.Exists(root)) return Array.Empty<ThemeInfo>();
            return Directory.EnumerateFiles(root, "*.json", SearchOption.TopDirectoryOnly)
                .Select(p => new ThemeInfo
                {
                    Id = Path.GetFileNameWithoutExtension(p),
                    Name = System.Globalization.CultureInfo.InvariantCulture.TextInfo.ToTitleCase(Path.GetFileNameWithoutExtension(p).Replace('_',' ').Replace('-',' ')),
                    Path = p
                })
                .OrderBy(x => x.Name)
                .ToArray();
        }

        public static void Apply(ThemeInfo theme)
        {
            if (Application.Current is null) return;
            if (!File.Exists(theme.Path)) return;
            using var doc = JsonDocument.Parse(File.ReadAllText(theme.Path));
            if (doc.RootElement.ValueKind != JsonValueKind.Object) return;
            foreach (var k in Keys)
            {
                if (doc.RootElement.TryGetProperty(k, out var el) && el.ValueKind == JsonValueKind.String)
                    Application.Current.Resources[k] = new SolidColorBrush(Color.Parse(el.GetString() ?? "#0F172A"));
            }
        }
    }
}
