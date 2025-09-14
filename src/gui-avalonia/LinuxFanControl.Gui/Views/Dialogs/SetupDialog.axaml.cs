#nullable enable
// (c) 2025 LinuxFanControl contributors. MIT License.
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Diagnostics;
using LinuxFanControl.Gui;
using LinuxFanControl.Gui.Services;
using LinuxFanControl.Gui.ViewModels.Dialogs;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    public partial class SetupDialog : Window
    {
        public SetupDialog()
        {
            InitializeComponent();
            DataContext = new SetupDialogViewModel();
            #if DEBUG
            this.AttachDevTools();
            #endif
        }

        private void OnOk(object? sender, RoutedEventArgs e)
        {
            if (DataContext is SetupDialogViewModel vm && !string.IsNullOrEmpty(vm.SelectedTheme))
            {
                var assetsRoot = AssetLocator.GetAssetsRoot();
                ThemeManager.ApplyTheme(assetsRoot, vm.SelectedTheme);
            }

            Close(true);
        }

        private void OnCancel(object? sender, RoutedEventArgs e)
        {
            Close(false);
        }
    }
}
