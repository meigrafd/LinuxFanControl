// (c) 2025 LinuxFanControl contributors. MIT License.
#nullable enable
using System;
using System.IO;

namespace LinuxFanControl.Gui
{
    public static class AssetLocator
    {
        public static string GetAssetsRoot()
        {
            var envPath = Environment.GetEnvironmentVariable("LFC_GUI_ASSETS_ROOT");
            if (!string.IsNullOrEmpty(envPath) && Directory.Exists(envPath))
                return envPath;

            var fallback = Path.Combine(AppContext.BaseDirectory, "Assets");
            if (Directory.Exists(fallback))
                return fallback;

            throw new DirectoryNotFoundException(
                $"Assets folder not found in '{fallback}' and env LFC_GUI_ASSETS_ROOT='{envPath}'.");
        }
    }
}
