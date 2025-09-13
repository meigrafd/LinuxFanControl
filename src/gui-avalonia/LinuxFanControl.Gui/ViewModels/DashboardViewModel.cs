// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels
{
    public sealed partial class DashboardViewModel : ObservableObject
    {
        public ObservableCollection<FanTileViewModel> Tiles { get; } = new();

        private readonly JsonRpcClient _rpc = new();
        private CancellationTokenSource? _cts;

        public DashboardViewModel()
        {
            StartPolling();
        }

        private void StartPolling()
        {
            _cts?.Cancel();
            _cts = new CancellationTokenSource();
            var ct = _cts.Token;

            _ = Task.Run(async () =>
            {
                while (!ct.IsCancellationRequested)
                {
                    try
                    {
                        // Expecting daemon method "telemetry.snapshot" -> array of fan objects
                        var arr = await _rpc.CallAsync<System.Text.Json.JsonElement[]>("telemetry.snapshot", ct: ct);
                        var list = arr is null
                        ? Array.Empty<FanSnapshot>()
                        : arr.Select(e => new FanSnapshot(
                            e.GetProperty("id").GetString() ?? "",
                                                          e.GetProperty("name").GetString() ?? "",
                                                          e.GetProperty("sensor").GetString() ?? "",
                                                          e.GetProperty("mode").GetString() ?? "Auto",
                                                          e.GetProperty("duty").GetInt32(),
                                                          e.GetProperty("rpm").GetInt32(),
                                                          e.GetProperty("temp").GetDouble()
                        )).ToArray();

                        UpdateTiles(list);
                    }
                    catch { /* swallow */ }

                    try { await Task.Delay(1000, ct); } catch { }
                }
            }, ct);
        }

        private void UpdateTiles(FanSnapshot[] snap)
        {
            foreach (var f in snap)
            {
                var tile = Tiles.FirstOrDefault(t => t.Id == f.Id);
                if (tile is null) { tile = new FanTileViewModel(f.Id); Tiles.Add(tile); }
                tile.UpdateFrom(f);
            }
            for (int i = Tiles.Count - 1; i >= 0; i--)
                if (!snap.Any(s => s.Id == Tiles[i].Id)) Tiles.RemoveAt(i);
        }
    }
}
