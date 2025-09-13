// (c) 2025 LinuxFanControl contributors. MIT License.
// VM driving the Setup dialog with streaming progress.

using System;
using System.Collections.ObjectModel;
using System.Threading;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels.Dialogs
{
    public partial class SetupDialogViewModel : ObservableObject
    {
        private readonly IDaemonClient _client;
        private CancellationTokenSource? _cts;

        [ObservableProperty] private bool isRunning;
        [ObservableProperty] private int progressValue;
        [ObservableProperty] private int progressMax = 100;
        public ObservableCollection<string> Lines { get; } = new();

        public SetupDialogViewModel()
        {
            _client = DaemonClient.Create();
        }

        [RelayCommand]
        public async Task StartAsync()
        {
            if (IsRunning) return;
            _cts = new CancellationTokenSource();
            IsRunning = true;
            ProgressValue = 0;
            Lines.Clear();

            try
            {
                await foreach (var line in _client.RunSetupAsync(_cts.Token))
                {
                    Lines.Add(line);
                    // naive progress guess
                    if (line.Contains('%'))
                    {
                        var idx = line.LastIndexOf('%');
                        var num = 0;
                        for (int i = idx - 1; i >= 0; i--)
                        {
                            if (!char.IsDigit(line[i])) break;
                            num = int.Parse(line.Substring(i, idx - i));
                            break;
                        }
                        ProgressValue = Math.Clamp(num, 0, ProgressMax);
                    }
                }
            }
            catch (OperationCanceledException) { }
            finally
            {
                IsRunning = false;
            }
        }

        [RelayCommand]
        public void Cancel()
        {
            if (!IsRunning) return;
            _cts?.Cancel();
            _ = _client.CancelSetupAsync();
        }
    }
}
