// (c) 2025 LinuxFanControl contributors. MIT License.
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using LinuxFanControl.Gui.ViewModels.Dialogs;

namespace LinuxFanControl.Gui.ViewModels.Dialogs
{
    public partial class SetupDialog : Window
    {
        public SetupDialog()
        {
            InitializeComponent();
            DataContext ??= new SetupDialogViewModel();
        }

        private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
    }
}
