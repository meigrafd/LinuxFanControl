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

            var files = Directory.GetFiles(themesPath, "*.json");
            return files
            .Select(f => Path.GetFileNameWithoutExtension(f)!)
            .ToArray();
        }
    }
}
