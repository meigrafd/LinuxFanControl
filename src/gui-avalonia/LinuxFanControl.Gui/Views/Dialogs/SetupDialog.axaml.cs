// (c) 2025 LinuxFanControl contributors. MIT License.
using Avalonia.Controls;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    public partial class SetupDialog : Window
    {
        public SetupDialog() => InitializeComponent();

        private void OnCancel(object? sender, Avalonia.Interactivity.RoutedEventArgs e) => Close(null);

        private void OnApply(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
        {
            if (DataContext is SetupDialogViewModel vm)
                Close(new SetupDialogResult { Theme = vm.SelectedTheme, Language = vm.SelectedLanguage, StartDetection = vm.RunDetection });
            else
                Close(null);
        }
    }

    public sealed class SetupDialogResult
    {
        public string Theme { get; set; } = "midnight";
        public string Language { get; set; } = "en";
        public bool StartDetection { get; set; }
    }
}
