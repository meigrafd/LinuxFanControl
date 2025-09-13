// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using Avalonia;                    // <-- needed for Application.Current
using Avalonia.Controls;
using Avalonia.Styling;
using LinuxFanControl.Gui.ViewModels;
using LinuxFanControl.Gui.Views.Dialogs;

namespace LinuxFanControl.Gui.Views
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();

            var vm = DataContext as MainWindowViewModel ?? new MainWindowViewModel();
            DataContext = vm;

            // If someone uses the VM commands instead of button Click handlers:
            vm.RequestSetup += (_, __) => OnSetupClicked(this, new Avalonia.Interactivity.RoutedEventArgs());
            vm.RequestImport += (_, __) => OnImportClicked(this, new Avalonia.Interactivity.RoutedEventArgs());
        }

        private async void OnSetupClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
        {
            var vm = DataContext as MainWindowViewModel ?? new MainWindowViewModel();
            var dlg = new SetupDialog
            {
                DataContext = new SetupDialogViewModel
                {
                    SelectedTheme = (Application.Current?.RequestedThemeVariant == ThemeVariant.Light) ? "Light" : "Dark",
                    SelectedLanguage = vm.SelectedLanguage ?? "en"
                }
            };

            var res = await dlg.ShowDialog<SetupDialogResult?>(this);
            if (res is null) return;

            Application.Current!.RequestedThemeVariant =
            string.Equals(res.Theme, "Light", StringComparison.OrdinalIgnoreCase)
            ? ThemeVariant.Light : ThemeVariant.Dark;

            vm.SelectedLanguage = res.Language;

            if (res.StartDetection)
            {
                vm.Dashboard.StatusMessage = "Starting detection… (daemon wiring pending)";
                // TODO: call daemon RPC detectCalibrate here
            }
        }

        private void OnImportClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
        {
            if (DataContext is MainWindowViewModel vm)
                vm.Dashboard.StatusMessage = "Import requested (dialog wiring pending).";
        }
    }
}
