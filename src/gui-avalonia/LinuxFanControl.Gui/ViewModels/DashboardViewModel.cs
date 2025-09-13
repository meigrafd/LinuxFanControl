// (c) 2025 LinuxFanControl contributors. MIT License.
// ViewModel for the main dashboard: top fan tiles + bottom editor tabs.

using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels
{
    public partial class DashboardViewModel : ObservableObject
    {
        private readonly IDaemonClient _client;
        private CancellationTokenSource? _cts;

        [ObservableProperty] private ObservableCollection<FanTileViewModel> fans = new();
        [ObservableProperty] private ObservableCollection<IEditorViewModel> editors = new();

        public DashboardViewModel()
        {
            _client = DaemonClient.Create();
            StartPolling();
        }

        private void StartPolling()
        {
            _cts?.Cancel();
            _cts = new CancellationTokenSource();
            var token = _cts.Token;

            // background poll (no UI freeze)
            Task.Run(async () =>
            {
                while (!token.IsCancellationRequested)
                {
                    try
                    {
                        var snapshot = await _client.GetFansSnapshotAsync(token);
                        App.UI(() =>
                        {
                            MergeFans(snapshot);
                        });
                    }
                    catch
                    {
                        // keep trying; mock client will continue producing data
                    }
                    await Task.Delay(1000, token);
                }
            }, token);
        }

        private void MergeFans(FanSnapshot[] items)
        {
            // simple reconcile by Id
            foreach (var vm in Fans.ToList())
            {
                if (!items.Any(i => i.Id == vm.Id))
                    Fans.Remove(vm);
            }
            foreach (var it in items)
            {
                var existing = Fans.FirstOrDefault(f => f.Id == it.Id);
                if (existing is null)
                {
                    existing = new FanTileViewModel(it.Id);
                    Fans.Add(existing);
                }
                existing.UpdateFrom(it);
            }
        }

        [RelayCommand]
        private void EditFan(FanTileViewModel fan)
        {
            if (fan is null) return;
            // avoid duplicate editors for same fan
            var existing = Editors.OfType<CurveEditorViewModel>().FirstOrDefault(e => e.FanId == fan.Id);
            if (existing is null)
            {
                var vm = new CurveEditorViewModel(fan.Id, fan.Name, _client);
                vm.CloseRequested += (_, __) => Editors.Remove(vm);
                Editors.Add(vm);
            }
        }

        [RelayCommand]
        private void CloseEditor(IEditorViewModel editor)
        {
            if (editor is null) return;
            Editors.Remove(editor);
        }

        public void Dispose()
        {
            _cts?.Cancel();
            _client.Dispose();
        }
    }
}
