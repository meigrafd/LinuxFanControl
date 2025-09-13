// (c) 2025 LinuxFanControl contributors. MIT License.
using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    public partial class SetupDialogViewModel : ObservableObject
    {
        public ObservableCollection<string> Themes { get; } = new() { "Dark", "Light" };
        public ObservableCollection<string> Languages { get; } = new() { "en", "de" };

        [ObservableProperty] private string? selectedTheme = "Dark";
        [ObservableProperty] private string? selectedLanguage = "en";
        [ObservableProperty] private bool runDetection = false;
    }
}
