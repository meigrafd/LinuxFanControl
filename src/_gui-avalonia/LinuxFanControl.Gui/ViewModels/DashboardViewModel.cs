// (c) 2025 LinuxFanControl contributors. MIT License.

using System.Collections.ObjectModel;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using ReactiveUI;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels
{
    public sealed class DashboardViewModel : ReactiveObject
    {
        private string _title = "Dashboard";
        public string Title { get => _title; set => this.RaiseAndSetIfChanged(ref _title, value); }

        public ObservableCollection<FanTileViewModel> FanTiles { get; } = new();

        public DashboardViewModel()
        {
            _ = LoadTilesAsync();
        }

        private async Task LoadTilesAsync()
        {
            // Populate from daemon (listChannels) if available; otherwise show empty dashboard.
            try
            {
                using var cts = new CancellationTokenSource(1500);
                var res = await RpcClient.Instance.ListChannelsAsync(cts.Token);
                if (res is JsonElement el && el.ValueKind == JsonValueKind.Array)
                {
                    foreach (var ch in el.EnumerateArray())
                    {
                        var id   = ch.TryGetProperty("id", out var pId) ? (pId.GetString() ?? "") : "";
                        var name = ch.TryGetProperty("name", out var pN) ? (pN.GetString() ?? id) : id;
                        var sensor = ch.TryGetProperty("sensor", out var pS) ? (pS.GetString() ?? "n/a") : "n/a";
                        FanTiles.Add(new FanTileViewModel
                        {
                            Id = string.IsNullOrEmpty(id) ? name : id,
                                     Name = name,
                                     SensorLabel = sensor
                        });
                    }
                }
            }
            catch
            {
                // ignore (daemon not running yet)
            }
        }
    }
}
