// (c) 2025 LinuxFanControl contributors. MIT License.
using System.Threading.Tasks;
using Avalonia.Controls;
using CommunityToolkit.Mvvm.Input;
using LinuxFanControl.Gui.Views.Dialogs;

namespace LinuxFanControl.Gui.ViewModels
{
    public partial class MainWindowViewModel
    {
        [RelayCommand]
        private async Task OpenSetupAsync(object? topLevel)
        {
            // topLevel should be a Window (MainWindow)
            if (topLevel is not Window owner) return;
            var dlg = new SetupDialog();
            await dlg.ShowAsync(owner);
        }

        [RelayCommand]
        private async Task OpenImportAsync(object? topLevel)
        {
            if (topLevel is not Window owner) return;
            var dlg = new ImportDialog();
            await dlg.ShowAsync(owner);
        }
    }
}
