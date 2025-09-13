// (c) 2025 LinuxFanControl contributors. MIT License.
// Drives the FanControl.Release import dialog.

using System.Collections.ObjectModel;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels.Dialogs
{
    public partial class ImportDialogViewModel : ObservableObject
    {
        [ObservableProperty] private string? filePath;
        [ObservableProperty] private bool canImport;
        public ObservableCollection<string> Warnings { get; } = new();

        [ObservableProperty] private bool importDone;
        [ObservableProperty] private string resultText = "";

        [RelayCommand]
        private void OnFilePicked(string? path)
        {
            FilePath = path;
            CanImport = !string.IsNullOrWhiteSpace(FilePath);
            ImportDone = false;
            Warnings.Clear();
            ResultText = "";
        }

        [RelayCommand]
        public async Task DoImportAsync()
        {
            if (string.IsNullOrWhiteSpace(FilePath)) return;
            var result = await FanControlImporter.ImportAsync(FilePath);
            foreach (var w in result.Warnings) Warnings.Add(w);

            await ConfigService.SaveAsync(result.Config);
            ImportDone = true;
            ResultText = $"Imported profile(s): {result.Config.Profiles.Length}. Saved to: {ConfigService.GetConfigPath()}";
        }
    }
}
