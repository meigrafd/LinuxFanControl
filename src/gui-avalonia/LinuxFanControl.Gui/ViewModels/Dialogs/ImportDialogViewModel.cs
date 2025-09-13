// (c) 2025 LinuxFanControl contributors. MIT License.
using System.IO;
using System.Threading.Tasks;
using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels.Dialogs
{
    public sealed partial class ImportDialogViewModel : ObservableObject
    {
        private readonly Window _window;

        [ObservableProperty] private string? selectedPath;
        [ObservableProperty] private string status = "";

        public ImportDialogViewModel(Window window) => _window = window;

        [RelayCommand]
        public async Task OnFilePicked(string? path)
        {
            if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
            {
                Status = "No file selected.";
                return;
            }

            SelectedPath = path;
            var importer = new FanControlImporter();
            var result = await importer.TryImportAsync(path);
            Status = result.Success ? "Imported." : $"Failed: {result.Error}";
        }
    }
}
