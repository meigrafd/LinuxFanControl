// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels.Dialogs
{
    public partial class SetupDialogViewModel : ObservableObject
    {
        private readonly Window _window;
        private readonly JsonRpcClient _rpc = new();
        private CancellationTokenSource? _cts;

        [ObservableProperty] private string logText = "";
        [ObservableProperty] private bool isRunning;

        public SetupDialogViewModel(Window window) => _window = window;

        [RelayCommand]
        private async Task StartAsync()
        {
            if (IsRunning) return;
            IsRunning = true;
            _cts = new CancellationTokenSource();

            try
            {
                var progress = new Progress<string>(ln => LogText += ln + Environment.NewLine);
                await _rpc.RunSetupAsync(progress, _cts.Token);
                LogText += "[done]" + Environment.NewLine;
            }
            catch (Exception ex)
            {
                LogText += "[ERROR] " + ex.Message + Environment.NewLine;
            }
            finally
            {
                IsRunning = false;
            }
        }

        [RelayCommand]
        private async Task CancelAsync()
        {
            try { await _rpc.CancelSetupAsync(); } catch { }
            try { _cts?.Cancel(); } catch { }
        }

        [RelayCommand]
        private void CloseWindow() => _window.Close();
    }
}
