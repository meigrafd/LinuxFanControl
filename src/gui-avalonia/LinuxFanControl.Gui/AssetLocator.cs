#nullable enable
// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using System.IO;

namespace LinuxFanControl.Gui
{
    public static class AssetLocator
    {
        public static string GetAssetsRoot()
        {
            var env = Environment.GetEnvironmentVariable("LFC_GUI_ASSETS_ROOT");
            if (!string.IsNullOrWhiteSpace(env) && Directory.Exists(env))
                return env;

            var baseDir = AppContext.BaseDirectory;
            var candidate = Path.Combine(baseDir, "Assets");
            if (Directory.Exists(candidate))
                return candidate;

            throw new DirectoryNotFoundException(
                $"Assets folder not found in '{baseDir}' and env LFC_GUI_ASSETS_ROOT='{env}'.");
        }
    }
}
