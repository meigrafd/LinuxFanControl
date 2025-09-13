// (c) 2025 LinuxFanControl contributors. MIT License.

using Avalonia.Controls;
using LinuxFanControl.Gui.Views.Dialogs;

namespace LinuxFanControl.Gui.Views
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
        }

        private async void OnSetupClick(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
        {
            var dlg = new SetupDialog();
            await dlg.ShowDialog<bool>(this);
        }

        private void OnQuitClick(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
        {
            Close();
        }
    }
}
