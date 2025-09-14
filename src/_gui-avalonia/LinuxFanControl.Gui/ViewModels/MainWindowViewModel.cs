// (c) 2025 LinuxFanControl contributors. MIT License.
using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;

namespace LinuxFanControl.Gui.ViewModels
{
    public partial class MainWindowViewModel : ObservableObject
    {
        public DashboardViewModel Dashboard { get; }

        // Keep language state here (used by Setup result)
        public ObservableCollection<string> Languages { get; } = new() { "en", "de" };
        [ObservableProperty] private string? selectedLanguage = "en";

        public MainWindowViewModel()
        {
            Dashboard = new DashboardViewModel();
            Dashboard.StatusMessage = "Ready. Click Setup to detect sensors & fans.";
        }
    }
}
