using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using Avalonia;
using Avalonia.Styling;

namespace LinuxFanControl.Gui.Services
{
    /// <summary>
    /// Applies themes dynamically. For now we switch Avalonia ThemeVariant (Light/Dark),
    /// and discover available themes by scanning Themes/*.json (names only, no hardcoding).
    /// </summary>
    public static class ThemeManager
    {
        public const string DefaultTheme = "dark";

        private static string ThemesRoot
        => Environment.GetEnvironmentVariable("LFC_THEMES_DIR")
        ?? Path.Combine(AppContext.BaseDirectory, "Themes");

        public static IReadOnlyList<string> ListThemes()
        {
            var list = new List<string>();
            if (Directory.Exists(ThemesRoot))
            {
                foreach (var f in Directory.EnumerateFiles(ThemesRoot, "*.json"))
                    list.Add(Path.GetFileNameWithoutExtension(f));
            }
            if (list.Count == 0) list.AddRange(new[] { "dark", "light" });
            return new ReadOnlyCollection<string>(list);
        }

        public static void ApplyTheme(string themeName)
        {
            var app = Application.Current;
            if (app is null) return;

            // Minimal reliable switch using Avalonia's ThemeVariant.
            var tn = (themeName ?? "").Trim().ToLowerInvariant();
            app.RequestedThemeVariant = tn == "light" ? ThemeVariant.Light : ThemeVariant.Dark;

            // NOTE: If you later want accent colors from Themes/{name}.json,
            // parse it here and put values into app.Resources.
        }
    }
}
