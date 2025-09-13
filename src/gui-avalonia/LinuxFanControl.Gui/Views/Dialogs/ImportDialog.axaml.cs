// (c) 2025 LinuxFanControl contributors. MIT License.
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Platform.Storage;
using Avalonia.Markup.Xaml;
using LinuxFanControl.Gui.ViewModels.Dialogs;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    public partial class ImportDialog : Window
    {
        public ImportDialog()
        {
            InitializeComponent();
            DataContext ??= new ImportDialogViewModel(this);
        }

        private void InitializeComponent() => AvaloniaXamlLoader.Load(this);

        // Helper for callers expecting dialog.ShowAsync(owner)
        public Task ShowAsync(Window owner) => this.ShowDialog(owner);

        private async void OnBrowseClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
        {
            if (DataContext is not ImportDialogViewModel vm) return;

            var files = await this.StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
            {
                Title = "Select FanControl config.json",
                AllowMultiple = false,
                FileTypeFilter = new[]
                {
                    new FilePickerFileType("JSON files"){ Patterns = new[]{ "*.json" } },
                                                                       new FilePickerFileType("All files"){ Patterns = new[]{ "*.*" } }
                }
            });

            var file = files?.FirstOrDefault();
            if (file is not null)
                await vm.OnFilePickedCommand.ExecuteAsync(file.Path.LocalPath);
        }

        private void OnCloseClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e) => Close();
    }
}
