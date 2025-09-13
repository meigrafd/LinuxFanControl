// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using CommunityToolkit.Mvvm.Input;

namespace LinuxFanControl.Gui.ViewModels
{
    // Keeps commands on the VM but lets the Window open dialogs.
    public partial class MainWindowViewModel
    {
        public event EventHandler? RequestSetup;
        public event EventHandler? RequestImport;

        private IRelayCommand? _setupCommand;
        public IRelayCommand SetupCommand => _setupCommand ??= new RelayCommand(() =>
        {
            // Notify view to open Setup dialog (no ShowAsync here).
            RequestSetup?.Invoke(this, EventArgs.Empty);
        });

        private IRelayCommand? _importCommand;
        public IRelayCommand ImportCommand => _importCommand ??= new RelayCommand(() =>
        {
            // Notify view to open Import dialog.
            RequestImport?.Invoke(this, EventArgs.Empty);
        });
    }
}
