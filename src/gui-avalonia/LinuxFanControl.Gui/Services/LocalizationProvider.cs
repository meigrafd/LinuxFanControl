// (c) 2025 LinuxFanControl contributors. MIT License.
// Simple runtime localization from Locales/*.json. No hardcodes.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace LinuxFanControl.Gui.Services
{
    public sealed class LocaleInfo
    {
        public string Id { get; init; } = "";   // e.g. "en", "de"
        public string Name { get; init; } = ""; // UI name from file, or TitleCase of Id
        public string Path { get; init; } = "";
        public override string ToString() => Name;
    }

    public sealed class LocalizationProvider
    {
        private static LocalizationProvider? _instance;
        public static LocalizationProvider Instance => _instance ??= new LocalizationProvider();

        private Dictionary<string, string> _strings = new(StringComparer.OrdinalIgnoreCase);
        public string this[string key] => _strings.TryGetValue(key, out var v) ? v : key;

        private LocalizationProvider() { }

        public IReadOnlyList<LocaleInfo> EnumerateLocales()
        {
            var root = GetLocalesRoot();
            if (!Directory.Exists(root)) return Array.Empty<LocaleInfo>();
            var list = new List<LocaleInfo>();
            foreach (var file in Directory.EnumerateFiles(root, "*.json", SearchOption.TopDirectoryOnly))
            {
                try
                {
                    var json = File.ReadAllText(file);
                    using var doc = JsonDocument.Parse(json);
                    var id = System.IO.Path.GetFileNameWithoutExtension(file);
                    var name = doc.RootElement.TryGetProperty("_name", out var n) && n.ValueKind == JsonValueKind.String
                    ? n.GetString() ?? id.ToUpperInvariant()
                    : id.ToUpperInvariant();
                    list.Add(new LocaleInfo { Id = id, Name = name, Path = file });
                }
                catch { }
            }
            return list.OrderBy(x => x.Name).ToArray();
        }

        public void Apply(LocaleInfo locale)
        {
            try
            {
                var json = File.ReadAllText(locale.Path);
                var dict = System.Text.Json.JsonSerializer.Deserialize<Dictionary<string, string>>(json)
                ?? new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                // Remove meta keys (like _name)
                dict.Remove("_name");
                _strings = new Dictionary<string, string>(dict, StringComparer.OrdinalIgnoreCase);
            }
            catch
            {
                _strings.Clear();
            }
        }

        public static string GetLocalesRoot()
        {
            // Relative to executable: ./Locales/*.json
            var baseDir = AppContext.BaseDirectory;
            return System.IO.Path.Combine(baseDir, "Locales");
        }
    }
}
