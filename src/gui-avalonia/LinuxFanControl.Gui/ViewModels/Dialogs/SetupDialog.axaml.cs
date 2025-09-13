// (c) 2025 LinuxFanControl contributors. MIT License.

using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using LinuxFanControl.Gui.ViewModels.Dialogs;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    public partial class SetupDialog : Window
    {
        public SetupDialog()
        {
            InitializeComponent();
            if (DataContext is not SetupDialogViewModel vm)
                DataContext = new SetupDialogViewModel();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }

        private void OnCloseClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
        {
            Close();
        }

        public async System.Threading.Tasks.Task RunAsync()
        {
            Show();
            if (DataContext is SetupDialogViewModel vm)
                await vm.StartAsyncCommand.ExecuteAsync(null);
        }
    }
}
