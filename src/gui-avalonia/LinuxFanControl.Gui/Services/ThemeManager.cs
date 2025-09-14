// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using System.IO;
using System.Linq;

namespace LinuxFanControl.Gui.Services
{
    public static class ThemeManager
    {
        private const string ThemesFolder = "Themes";

        /// <summary>
        /// Returns the names (without “.json”) of all theme files in Assets/Themes.
        /// </summary>
        public static string[] GetAvailableThemes(string assetsRoot)
        {
            var themesPath = Path.Combine(assetsRoot, ThemesFolder);
            if (!Directory.Exists(themesPath))
                return Array.Empty<string>();

            return Directory
            .GetFiles(themesPath, "*.json")
            .Select(Path.GetFileNameWithoutExtension)
            .ToArray();
        }

        /// <summary>
        /// Alias for GetAvailableThemes, used by ViewModels.
        /// </summary>
        public static string[] ListThemes(string assetsRoot) =>
        GetAvailableThemes(assetsRoot);

        /// <summary>
        /// Applies the given theme at runtime.
        /// </summary>
        public static void ApplyTheme(string assetsRoot, string themeName)
        {
            var file = Path.Combine(assetsRoot, ThemesFolder, themeName + ".json");
            if (!File.Exists(file))
                return;

            // TODO: Parse JSON and inject into Application.Current.Styles
            Console.WriteLine($"ApplyTheme called for '{themeName}' at '{file}'");
        }
    }
}
