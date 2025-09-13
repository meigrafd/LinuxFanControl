// (c) 2025 LinuxFanControl contributors. MIT License.
using Avalonia.Controls;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    public sealed class SetupDialogResult
    {
        public string Theme { get; init; } = "Dark";
        public string Language { get; init; } = "en";
        public bool StartDetection { get; init; }
    }

    public partial class SetupDialog : Window
    {
        public SetupDialog()
        {
            InitializeComponent();
        }

        private void OnCancel(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
        => Close(null);

        private void OnApply(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
        {
            if (DataContext is not SetupDialogViewModel vm)
            {
                Close(null);
                return;
            }
            Close(new SetupDialogResult
            {
                Theme = vm.SelectedTheme ?? "Dark",
                Language = vm.SelectedLanguage ?? "en",
                StartDetection = vm.RunDetection
            });
        }
    }
}
