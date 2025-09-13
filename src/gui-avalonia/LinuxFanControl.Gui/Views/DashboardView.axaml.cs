// (c) 2025 LinuxFanControl contributors. MIT License.
using Avalonia.Controls;
using Avalonia.Styling;
using LinuxFanControl.Gui.ViewModels;
using LinuxFanControl.Gui.Views.Dialogs;
using System;

namespace LinuxFanControl.Gui.Views
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            DataContext ??= new MainWindowViewModel();
        }

        private async void OnSetupClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
        {
            var vm = DataContext as MainWindowViewModel ?? new MainWindowViewModel();
            var dlg = new SetupDialog
            {
                DataContext = new SetupDialogViewModel
                {
                    // current selections
                    SelectedTheme = (Application.Current?.RequestedThemeVariant == ThemeVariant.Light) ? "Light" : "Dark",
                    SelectedLanguage = vm.SelectedLanguage ?? "en"
                }
            };

            var res = await dlg.ShowDialog<SetupDialogResult?>(this);
            if (res is null) return;

            // Apply theme
            Application.Current!.RequestedThemeVariant =
            string.Equals(res.Theme, "Light", StringComparison.OrdinalIgnoreCase)
            ? ThemeVariant.Light : ThemeVariant.Dark;

            // Apply language (simple: just store; binding texts can be wired next step)
            vm.SelectedLanguage = res.Language;

            if (res.StartDetection)
            {
                vm.Dashboard.StatusMessage = "Starting detection… (to be wired to daemon)";
                // TODO: invoke daemon RPC detectCalibrate here
            }
        }

        private void OnImportClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
        {
            var vm = DataContext as MainWindowViewModel ?? new MainWindowViewModel();
            vm.Dashboard.StatusMessage = "Import requested (dialog wiring next step).";
        }
    }
}
