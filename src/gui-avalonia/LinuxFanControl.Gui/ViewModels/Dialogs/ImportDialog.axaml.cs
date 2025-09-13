// (c) 2025 LinuxFanControl contributors. MIT License.

using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using LinuxFanControl.Gui.ViewModels.Dialogs;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    public partial class ImportDialog : Window
    {
        public ImportDialog()
        {
            InitializeComponent();
            if (DataContext is not ImportDialogViewModel vm)
                DataContext = new ImportDialogViewModel();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }

        private async void OnBrowse(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
        {
            if (DataContext is not ImportDialogViewModel vm) return;

            var ofd = new OpenFileDialog
            {
                Title = "Select FanControl JSON",
                AllowMultiple = false,
                Filters =
                {
                    new FileDialogFilter{ Name = "JSON", Extensions = { "json" } },
                    new FileDialogFilter{ Name = "All files", Extensions = { "*" } },
                }
            };

            var paths = await ofd.ShowAsync(this);
            if (paths is { Length: > 0 })
                vm.OnFilePickedCommand.Execute(paths[0]);
        }

        private void OnClose(object? sender, Avalonia.Interactivity.RoutedEventArgs e) => Close();

        public static async Task ShowAsync(Window owner)
        {
            var dlg = new ImportDialog { CanResize = true };
            await dlg.ShowDialog(owner);
        }
    }
}
