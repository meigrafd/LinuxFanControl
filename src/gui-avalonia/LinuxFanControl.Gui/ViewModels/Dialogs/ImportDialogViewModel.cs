// (c) 2025 LinuxFanControl contributors. MIT License.
using System.IO;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels.Dialogs
{
    public partial class ImportDialogViewModel : ObservableObject
    {
        [ObservableProperty] private string? selectedPath;
        [ObservableProperty] private string status = "";

        [RelayCommand]
        private async Task OnFilePickedAsync(string? path)
        {
            if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
            {
                Status = "No file selected.";
                return;
            }

            SelectedPath = path;
            var importer = new FanControlImporter();
            var result = await importer.TryImportAsync(path);
            Status = result.Success ? "Imported." : "Failed to import.";
        }
    }
}
