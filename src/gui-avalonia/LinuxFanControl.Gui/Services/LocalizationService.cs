// (c) 2025 LinuxFanControl contributors. MIT License.
using System.IO;
using System.Text.Json;
using System.Collections.Generic;

namespace LinuxFanControl.Gui.Services
{
    public sealed class LocalizationService
    {
        public static LocalizationService Instance { get; } = new();
        public string CurrentLanguage { get; private set; } = "en";
        private Dictionary<string,string> _strings = new();

        private LocalizationService() { Load("en"); }
        public void SetLanguage(string lang) => Load(lang);

        private void Load(string lang)
        {
            var baseDir = System.AppContext.BaseDirectory;
            var path = Path.Combine(baseDir, "Locales", $"{lang}.json");
            if (!File.Exists(path)) lang = "en";
            path = Path.Combine(baseDir, "Locales", $"{lang}.json");
            CurrentLanguage = lang;
            var json = File.ReadAllText(path);
            _strings = JsonSerializer.Deserialize<Dictionary<string,string>>(json) ?? new();
        }

        public Localizer Localizer => new(_strings);
    }

    public readonly struct Localizer
    {
        private readonly IReadOnlyDictionary<string,string> _map;
        public Localizer(IReadOnlyDictionary<string,string> map) { _map = map; }
        public string this[string key] => _map.TryGetValue(key, out var v) ? v : key;
        public string SetupTitle => this["setup.title"];
        public string SetupHeadline => this["setup.headline"];
        public string Theme => this["setup.theme"];
        public string Language => this["setup.language"];
        public string RunDetection => this["setup.run_detection"];
        public string ImportFanControl => this["setup.import_fc"];
        public string ImportHint => this["setup.import_hint"];
        public string Cancel => this["common.cancel"];
        public string Apply => this["common.apply"];
    }
}
