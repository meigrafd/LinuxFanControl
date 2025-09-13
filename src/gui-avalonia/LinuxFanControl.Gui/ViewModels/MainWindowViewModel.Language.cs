// (c) 2025 LinuxFanControl contributors. MIT License.
using System.Collections.ObjectModel;
using System.Linq;
using CommunityToolkit.Mvvm.ComponentModel;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels
{
    public partial class MainWindowViewModel : ObservableObject
    {
        [ObservableProperty] private ObservableCollection<LocalizationService.LanguageInfo> languages = new();
        [ObservableProperty] private LocalizationService.LanguageInfo? selectedLanguage;

        partial void OnSelectedLanguageChanged(LocalizationService.LanguageInfo? value)
        {
            if (value is null) return;
            LocalizationService.Instance.Load(value.Code);
        }

        public void EnsureLanguagesInitialized()
        {
            if (Languages.Count > 0) return;

            var discovered = LocalizationService.Discover();
            foreach (var l in discovered) Languages.Add(l);

            var (code, _) = LocalizationService.LoadGuiConfigOrDefault();
            SelectedLanguage =
            Languages.FirstOrDefault(l => l.Code == code) ??
            Languages.FirstOrDefault(l => l.Code == "en") ??
            Languages.FirstOrDefault();

            if (SelectedLanguage != null && LocalizationService.Instance.CurrentCode != SelectedLanguage.Code)
                LocalizationService.Instance.Load(SelectedLanguage.Code);
        }
    }
}
