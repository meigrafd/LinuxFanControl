// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using System.IO;
using System.Text.Json;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Media;

namespace LinuxFanControl.Gui.Services
{
    /// <summary>Loads theme JSON from Themes/{key}.json at runtime.</summary>
    public sealed class ThemeService
    {
        public static ThemeService Instance { get; } = new();
        public string CurrentThemeKey { get; private set; } = "midnight";
        private ThemeService() {}

        public async Task ApplyAsync(string key)
        {
            var baseDir = AppContext.BaseDirectory;
            var path = Path.Combine(baseDir, "Themes", $"{key}.json");
            if (!File.Exists(path)) throw new FileNotFoundException($"Theme not found: {path}");

            using var s = File.OpenRead(path);
            var json = await JsonDocument.ParseAsync(s);
            var root = json.RootElement;

            void Set(string name, string hex) => Application.Current!.Resources[name] = SolidColorBrush.Parse(hex);

            CurrentThemeKey = key;

            Set("Foreground", root.GetProperty("Foreground").GetString()!);
            Set("SubtleText", root.GetProperty("SubtleText").GetString()!);

            Set("TileBg", root.GetProperty("TileBg").GetString()!);
            Set("TileBorder", root.GetProperty("TileBorder").GetString()!);

            Set("ButtonBg", root.GetProperty("ButtonBg").GetString()!);
            Set("ButtonBgHover", root.GetProperty("ButtonBgHover").GetString()!);
            Set("ButtonText", root.GetProperty("ButtonText").GetString()!);
            Set("ButtonBorder", root.GetProperty("ButtonBorder").GetString()!);

            Set("AccentBrush", root.GetProperty("AccentBrush").GetString()!);
            Set("AccentBorder", root.GetProperty("AccentBorder").GetString()!);

            Set("FanLogo.Back", root.GetProperty("FanLogoBack").GetString()!);
            Set("FanLogo.Blade", root.GetProperty("FanLogoBlade").GetString()!);
            Set("FanLogo.Hub", root.GetProperty("FanLogoHub").GetString()!);
        }
    }
}
