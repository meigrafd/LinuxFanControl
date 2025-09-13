// (c) 2025 LinuxFanControl contributors. MIT License.
using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    public partial class SetupDialogViewModel : ObservableObject
    {
        public ObservableCollection<string> Themes { get; } = new(["midnight","light"]);
        public ObservableCollection<string> Languages { get; } = new(["en","de"]);

        [ObservableProperty] private string _selectedTheme = "midnight";
        [ObservableProperty] private string _selectedLanguage = "en";
        [ObservableProperty] private bool _runDetection = true;

        public Localizer Loc { get; } = LocalizationService.Instance.Localizer;

        public IRelayCommand ImportCommand => new RelayCommand(() => { /* TODO */ });
    }
}
