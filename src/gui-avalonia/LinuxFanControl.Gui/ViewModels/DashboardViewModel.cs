// (c) 2025 LinuxFanControl contributors. MIT License.
using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;

namespace LinuxFanControl.Gui.ViewModels
{
    public partial class DashboardViewModel : ObservableObject
    {
        public ObservableCollection<FanTileViewModel> FanTiles { get; } = new();

        [ObservableProperty] private string statusMessage = string.Empty;

        public DashboardViewModel()
        {
            // No automatic detection here; keep empty and show placeholder.
            // If you want a temporary visual check, uncomment:
            // FanTiles.Add(new FanTileViewModel("CPU Fan", 950, 35, "44.1 °C"));
        }
    }
}
