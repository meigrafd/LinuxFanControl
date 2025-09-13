// (c) 2025 LinuxFanControl contributors. MIT License.
// Main VM: loads config, applies theme & language, polls daemon telemetry, profile management, import/export commands.

using System;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels
{
    public sealed partial class MainWindowViewModel : ObservableObject
    {
        // Collections for top bar dropdowns
        public ObservableCollection<LocaleInfo> Languages { get; } = new();
        public ObservableCollection<ThemeInfo> Themes { get; } = new();

        [ObservableProperty] private LocaleInfo? selectedLanguage;
        [ObservableProperty] private ThemeInfo? selectedTheme;

        // Fan tiles (shown in Dashboard)
        public ObservableCollection<FanTileViewModel> Tiles { get; } = new();

        // Profiles (from AppConfig)
        public ObservableCollection<Profile> Profiles { get; } = new();
        [ObservableProperty] private Profile? currentProfile;

        // Services
        private readonly IDaemonClient _daemon = DaemonClient.Create();
        private AppConfig _cfg = new();
        private CancellationTokenSource? _pollCts;

        public MainWindowViewModel()
        {
            // Kick async init
            _ = InitializeAsync();
        }

        // ---------------- Initialization ----------------
        private async Task InitializeAsync()
        {
            // Load config
            _cfg = await ConfigService.LoadAsync();

            // Load and apply locales
            Languages.Clear();
            foreach (var l in LocalizationProvider.Instance.EnumerateLocales())
                Languages.Add(l);
            var lang = Languages.FirstOrDefault(x => string.Equals(x.Id, _cfg.SelectedLanguage, StringComparison.OrdinalIgnoreCase))
            ?? Languages.FirstOrDefault()
            ?? new LocaleInfo { Id = "en", Name = "English", Path = Path.Combine(LocalizationProvider.GetLocalesRoot(), "en.json") };
            SelectedLanguage = lang;
            LocalizationProvider.Instance.Apply(lang);

            // Load and apply themes
            Themes.Clear();
            foreach (var t in ThemeService.Enumerate())
                Themes.Add(t);
            var theme = Themes.FirstOrDefault(x => string.Equals(x.Id, _cfg.SelectedTheme, StringComparison.OrdinalIgnoreCase))
            ?? Themes.FirstOrDefault()
            ?? new ThemeInfo { Id = "midnight", Name = "Midnight", Path = Path.Combine(ThemeService.GetThemeRoot(), "midnight.json") };
            SelectedTheme = theme;
            ThemeService.Apply(theme);

            // Profiles from config
            Profiles.Clear();
            foreach (var p in _cfg.Profiles) Profiles.Add(p);
            CurrentProfile = Profiles.FirstOrDefault() ?? new Profile { Name = "Default", Channels = Array.Empty<Channel>() };
            if (!Profiles.Contains(CurrentProfile)) Profiles.Add(CurrentProfile);

            // Start telemetry polling
            StartPolling();
        }

        partial void OnSelectedLanguageChanged(LocaleInfo? value)
        {
            if (value is null) return;
            LocalizationProvider.Instance.Apply(value);
            _cfg.SelectedLanguage = value.Id;
            _ = ConfigService.SaveAsync(_cfg);
        }

        partial void OnSelectedThemeChanged(ThemeInfo? value)
        {
            if (value is null) return;
            ThemeService.Apply(value);
            _cfg.SelectedTheme = value.Id;
            _ = ConfigService.SaveAsync(_cfg);
        }

        partial void OnCurrentProfileChanged(Profile? value)
        {
            // future: reflect channels etc.
        }

        // ---------------- Telemetry polling ----------------
        private void StartPolling()
        {
            _pollCts?.Cancel();
            _pollCts = new CancellationTokenSource();
            var ct = _pollCts.Token;
            _ = Task.Run(async () =>
            {
                while (!ct.IsCancellationRequested)
                {
                    try
                    {
                        var snap = await _daemon.GetFansSnapshotAsync(ct);
                        UpdateTiles(snap);
                    }
                    catch { /* ignore */ }

                    try { await Task.Delay(1000, ct); } catch { }
                }
            }, ct);
        }

        private void UpdateTiles(FanSnapshot[] snap)
        {
            // Ensure stable order by Id
            Array.Sort(snap, (a,b) => string.CompareOrdinal(a.Id, b.Id));
            // Add missing tiles
            foreach (var s in snap)
            {
                var tile = Tiles.FirstOrDefault(x => x.Id == s.Id);
                if (tile is null)
                {
                    tile = new FanTileViewModel(s.Id) { Name = s.Name };
                    Tiles.Add(tile);
                }
                tile.UpdateFrom(s);
            }
            // Remove tiles no longer present
            for (int i = Tiles.Count - 1; i >= 0; i--)
            {
                if (!snap.Any(s => s.Id == Tiles[i].Id))
                    Tiles.RemoveAt(i);
            }
        }

        // ---------------- Commands: Setup/Import/Export ----------------
        [RelayCommand]
        private async Task Export()
        {
            try
            {
                var path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Desktop), "LinuxFanControl.config.json");
                await ConfigService.SaveAsync(_cfg);
                File.Copy(ConfigService.GetConfigPath(), path, overwrite: true);
            }
            catch
            {
                // swallow for now
            }
        }

        // Setup & Import Commands are defined in the Commands partial (previous step).

        // ---------------- Profile management ----------------
        [RelayCommand]
        private async Task NewProfile()
        {
            var name = $"Profile {Profiles.Count + 1}";
            var p = new Profile { Name = name, Channels = Array.Empty<Channel>() };
            Profiles.Add(p);
            CurrentProfile = p;
            _cfg.Profiles = Profiles.ToArray();
            await ConfigService.SaveAsync(_cfg);
        }

        [RelayCommand]
        private async Task DuplicateProfile()
        {
            if (CurrentProfile is null) return;
            var dup = new Profile
            {
                Name = CurrentProfile.Name + " (Copy)",
                Channels = CurrentProfile.Channels.Select(c => new Channel
                {
                    Name = c.Name,
                    Sensor = c.Sensor,
                    Output = c.Output,
                    Mode = c.Mode,
                    HysteresisC = c.HysteresisC,
                    ResponseTauS = c.ResponseTauS,
                    Curve = c.Curve.Select(p => new CurvePoint { X = p.X, Y = p.Y }).ToArray()
                }).ToArray()
            };
            Profiles.Add(dup);
            CurrentProfile = dup;
            _cfg.Profiles = Profiles.ToArray();
            await ConfigService.SaveAsync(_cfg);
        }

        [RelayCommand]
        private async Task RenameProfile(string? newName)
        {
            if (CurrentProfile is null || string.IsNullOrWhiteSpace(newName)) return;
            CurrentProfile.Name = newName;
            _cfg.Profiles = Profiles.ToArray();
            await ConfigService.SaveAsync(_cfg);
            OnPropertyChanged(nameof(Profiles));
        }

        [RelayCommand]
        private async Task DeleteProfile()
        {
            if (CurrentProfile is null) return;
            var idx = Profiles.IndexOf(CurrentProfile);
            Profiles.Remove(CurrentProfile);
            CurrentProfile = Profiles.ElementAtOrDefault(Math.Max(0, idx - 1));
            _cfg.Profiles = Profiles.ToArray();
            await ConfigService.SaveAsync(_cfg);
        }
    }
}
