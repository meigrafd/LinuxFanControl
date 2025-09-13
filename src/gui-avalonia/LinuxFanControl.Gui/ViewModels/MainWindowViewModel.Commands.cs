// (c) 2025 LinuxFanControl contributors. MIT License.

using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using LinuxFanControl.Gui.Views.Dialogs;
using Avalonia;

namespace LinuxFanControl.Gui.ViewModels
{
    public partial class MainWindowViewModel : ObservableObject
    {
        [RelayCommand]
        private async Task Setup()
        {
            // open non-blocking setup dialog
            if (Application.Current?.ApplicationLifetime is Avalonia.Controls.ApplicationLifetimes.IClassicDesktopStyleApplicationLifetime life
                && life.MainWindow is not null)
            {
                var dlg = new SetupDialog();
                await dlg.RunAsync();
            }
        }

        [RelayCommand]
        private async Task Import()
        {
            if (Application.Current?.ApplicationLifetime is Avalonia.Controls.ApplicationLifetimes.IClassicDesktopStyleApplicationLifetime life
                && life.MainWindow is not null)
            {
                await ImportDialog.ShowAsync(life.MainWindow);
            }
        }
    }
}
