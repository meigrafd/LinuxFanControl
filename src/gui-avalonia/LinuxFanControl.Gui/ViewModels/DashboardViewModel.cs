// (c) 2025 LinuxFanControl contributors. MIT License.
using System.Collections.ObjectModel;
using System.Collections.Specialized;
using CommunityToolkit.Mvvm.ComponentModel;

namespace LinuxFanControl.Gui.ViewModels
{
    public partial class DashboardViewModel : ObservableObject
    {
        public ObservableCollection<FanTileViewModel> FanTiles { get; } = new();

        [ObservableProperty] private string statusMessage = string.Empty;
        [ObservableProperty] private bool hasTiles;

        public DashboardViewModel()
        {
            HasTiles = FanTiles.Count > 0;
            FanTiles.CollectionChanged += OnTilesChanged;

            // Für schnellen Sichttest: eine Demo-Kachel aktivieren (später vom Daemon befüllt)
            // FanTiles.Add(new FanTileViewModel("CPU Fan", 950, 35, "44.1 °C"));
        }

        private void OnTilesChanged(object? sender, NotifyCollectionChangedEventArgs e)
        => HasTiles = FanTiles.Count > 0;
    }
}
