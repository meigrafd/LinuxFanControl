// (c) 2025 LinuxFanControl contributors. MIT License.
// Minimal app config loader/saver (JSON) with version tag.

using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Tasks;

namespace LinuxFanControl.Gui.Services
{
    public static class ConfigService
    {
        private static readonly JsonSerializerOptions Json = new(JsonSerializerDefaults.Web)
        {
            DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
            WriteIndented = true
        };

        public const string CurrentVersion = "0.1.0";

        public static string GetConfigPath()
        {
            var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            var xdg = Environment.GetEnvironmentVariable("XDG_CONFIG_HOME");
            var dir = !string.IsNullOrWhiteSpace(xdg)
            ? Path.Combine(xdg, "LinuxFanControl")
            : Path.Combine(home, ".config", "LinuxFanControl");
            Directory.CreateDirectory(dir);
            return Path.Combine(dir, "config.json");
        }

        public static async Task<AppConfig> LoadAsync()
        {
            var path = GetConfigPath();
            if (!File.Exists(path)) return new AppConfig { Version = CurrentVersion };
            try
            {
                await using var fs = File.OpenRead(path);
                var cfg = await JsonSerializer.DeserializeAsync<AppConfig>(fs, Json);
                return cfg ?? new AppConfig { Version = CurrentVersion };
            }
            catch
            {
                return new AppConfig { Version = CurrentVersion };
            }
        }

        public static async Task SaveAsync(AppConfig cfg)
        {
            cfg.Version ??= CurrentVersion;
            var path = GetConfigPath();
            await using var fs = File.Create(path);
            await JsonSerializer.SerializeAsync(fs, cfg, Json);
        }
    }

    // ---------------- DTOs for app config ----------------

    public sealed class AppConfig
    {
        public string? Version { get; set; }
        public string? SelectedTheme { get; set; }
        public string? SelectedLanguage { get; set; }
        public Profile[] Profiles { get; set; } = Array.Empty<Profile>();
    }

    public sealed class Profile
    {
        public string Name { get; set; } = "Default";
        public Channel[] Channels { get; set; } = Array.Empty<Channel>();
    }

    public sealed class Channel
    {
        public string Name { get; set; } = "";
        public string Sensor { get; set; } = "";  // logical sensor label
        public string Output { get; set; } = "";  // pwm identifier
        public string Mode { get; set; } = "Auto";
        public double HysteresisC { get; set; }
        public double ResponseTauS { get; set; }
        public CurvePoint[] Curve { get; set; } = Array.Empty<CurvePoint>();
    }

    public sealed class CurvePoint
    {
        public double X { get; set; }
        public double Y { get; set; }
    }
}
