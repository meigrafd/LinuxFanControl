using System;
using System.IO;
using System.Text.Json;

namespace LinuxFanControl.Gui.Services
{
    // Simple persisted GUI settings (language/theme)
    public record GuiConfig(string Language, string Theme)
    {
        public static GuiConfig Default => new("en", "midnight");
    }

    public static class ConfigService
    {
        static string ConfigDir => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".config", "lfc");
        static string ConfigPath => Path.Combine(ConfigDir, "gui.json");

        public static GuiConfig Load()
        {
            try
            {
                if (File.Exists(ConfigPath))
                {
                    var json = File.ReadAllText(ConfigPath);
                    var cfg = JsonSerializer.Deserialize<GuiConfig>(json);
                    if (cfg != null) return cfg;
                }
            }
            catch { }
            return GuiConfig.Default;
        }

        public static void Save(GuiConfig cfg)
        {
            try
            {
                Directory.CreateDirectory(ConfigDir);
                var json = JsonSerializer.Serialize(cfg, new JsonSerializerOptions{ WriteIndented = true });
                File.WriteAllText(ConfigPath, json);
            }
            catch { }
        }
    }
}
