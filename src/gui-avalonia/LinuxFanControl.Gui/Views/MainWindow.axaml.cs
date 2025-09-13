// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Styling;
using LinuxFanControl.Gui.ViewModels;
using LinuxFanControl.Gui.Views.Dialogs;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.Views
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            var vm = DataContext as MainWindowViewModel ?? new MainWindowViewModel();
            DataContext = vm;
            vm.RequestSetup += (_, __) => OnSetupClicked(this, new Avalonia.Interactivity.RoutedEventArgs());
        }

        private async void OnSetupClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
        {
            var dlg = new SetupDialog
            {
                DataContext = new SetupDialogViewModel
                {
                    SelectedTheme = ThemeService.Instance.CurrentThemeKey,
                    SelectedLanguage = LocalizationService.Instance.CurrentLanguage
                }
            };

            var res = await dlg.ShowDialog<SetupDialogResult?>(this);
            if (res is null) return;

            await ThemeService.Instance.ApplyAsync(res.Theme);
            LocalizationService.Instance.SetLanguage(res.Language);
            Application.Current!.RequestedThemeVariant =
                string.Equals(res.Theme, "light", StringComparison.OrdinalIgnoreCase)
                ? ThemeVariant.Light : ThemeVariant.Dark;

            if (DataContext is MainWindowViewModel vm && res.StartDetection)
            {
                vm.Dashboard.StatusMessage = "Starting detection…";
                // TODO: daemon RPC detectCalibrate
            }
        }
    }
}
