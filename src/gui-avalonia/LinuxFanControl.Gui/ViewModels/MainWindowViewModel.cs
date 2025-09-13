// (c) 2025 LinuxFanControl contributors. MIT License.
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
        public ObservableCollection<LocaleInfo> Languages { get; } = new();
        public ObservableCollection<ThemeInfo> Themes { get; } = new();
        [ObservableProperty] private LocaleInfo? selectedLanguage;
        [ObservableProperty] private ThemeInfo? selectedTheme;

        public ObservableCollection<FanTileViewModel> Tiles { get; } = new();
        public ObservableCollection<Profile> Profiles { get; } = new();
        [ObservableProperty] private Profile? currentProfile;

        private readonly IDaemonClient _daemon = DaemonClient.Create();
        private AppConfig _cfg = new();
        private CancellationTokenSource? _pollCts;

        public MainWindowViewModel() { _ = InitializeAsync(); }

        private async Task InitializeAsync()
        {
            _cfg = await ConfigService.LoadAsync();

            Languages.Clear();
            foreach (var l in LocalizationProvider.Instance.EnumerateLocales()) Languages.Add(l);
            SelectedLanguage = Languages.FirstOrDefault(x => string.Equals(x.Id, _cfg.SelectedLanguage, StringComparison.OrdinalIgnoreCase)) ?? Languages.FirstOrDefault();
            if (SelectedLanguage is not null) LocalizationProvider.Instance.Apply(SelectedLanguage);

            Themes.Clear();
            foreach (var t in ThemeService.Enumerate()) Themes.Add(t);
            SelectedTheme = Themes.FirstOrDefault(x => string.Equals(x.Id, _cfg.SelectedTheme, StringComparison.OrdinalIgnoreCase)) ?? Themes.FirstOrDefault();
            if (SelectedTheme is not null) ThemeService.Apply(SelectedTheme);

            Profiles.Clear();
            foreach (var p in _cfg.Profiles) Profiles.Add(p);
            CurrentProfile = Profiles.FirstOrDefault() ?? new Profile { Name = "Default" };
            if (!Profiles.Contains(CurrentProfile)) Profiles.Add(CurrentProfile);

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

        private void StartPolling()
        {
            _pollCts?.Cancel();
            _pollCts = new CancellationTokenSource();
            var ct = _pollCts.Token;
            _ = Task.Run(async () =>
            {
                while (!ct.IsCancellationRequested)
                {
                    try { UpdateTiles(await _daemon.GetFansSnapshotAsync(ct)); } catch { }
                    try { await Task.Delay(1000, ct); } catch { }
                }
            }, ct);
        }

        private void UpdateTiles(FanSnapshot[] snap)
        {
            Array.Sort(snap, (a,b) => string.CompareOrdinal(a.Id, b.Id));
            foreach (var s in snap)
            {
                var tile = Tiles.FirstOrDefault(x => x.Id == s.Id);
                if (tile is null) { tile = new FanTileViewModel(s.Id) { Name = s.Name }; Tiles.Add(tile); }
                tile.UpdateFrom(s);
            }
            for (int i = Tiles.Count - 1; i >= 0; i--)
                if (!snap.Any(s => s.Id == Tiles[i].Id)) Tiles.RemoveAt(i);
        }

        [RelayCommand] private async Task Export()
        {
            try
            {
                await ConfigService.SaveAsync(_cfg);
                var dst = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Desktop), "LinuxFanControl.config.json");
                File.Copy(ConfigService.GetConfigPath(), dst, true);
            }
            catch { }
        }
    }
}
