// (c) 2025 LinuxFanControl contributors. MIT License.
using System.IO;
using System.Threading.Tasks;
using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels.Dialogs
{
    /// <summary>
    /// ViewModel for importing a FanControl.Release JSON config.
    /// Uses explicit IAsyncRelayCommand to avoid source-generator dependency.
    /// </summary>
    public sealed class ImportDialogViewModel : ObservableObject
    {
        private readonly Window _window;

        private string? _selectedPath;
        public string? SelectedPath
        {
            get => _selectedPath;
            set => SetProperty(ref _selectedPath, value);
        }

        private string _status = "";
        public string Status
        {
            get => _status;
            set => SetProperty(ref _status, value);
        }

        public IAsyncRelayCommand<string?> OnFilePickedCommand { get; }

        public ImportDialogViewModel(Window window)
        {
            _window = window;
            OnFilePickedCommand = new AsyncRelayCommand<string?>(OnFilePickedAsync);
        }

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

            Status = result.Success ? "Imported." : $"Failed: {result.Error}";
            // TODO: merge result.Config into current app config and persist if needed.
        }
    }
}
