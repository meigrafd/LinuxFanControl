// (c) 2025 LinuxFanControl contributors. MIT License.
using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace LinuxFanControl.Gui.ViewModels
{
    public partial class MainWindowViewModel : ObservableObject
    {
        public DashboardViewModel Dashboard { get; }

        // top bar
        public ObservableCollection<string> Profiles { get; } = new() { "Default" };
        [ObservableProperty] private string? selectedProfile = "Default";

        public ObservableCollection<string> Languages { get; } = new() { "en", "de" };
        [ObservableProperty] private string? selectedLanguage = "en";

        public MainWindowViewModel()
        {
            Dashboard = new DashboardViewModel();
            // Optional: some initial hint
            Dashboard.StatusMessage = "Ready. Click Setup to detect sensors & fans.";
        }

        [RelayCommand]
        private void Setup()
        {
            // Show detect dialog (to be wired to daemon)
            Dashboard.StatusMessage = "Setup requested. Detection dialog not yet wired.";
        }

        [RelayCommand]
        private void Import()
        {
            Dashboard.StatusMessage = "Import requested.";
        }
    }
}
