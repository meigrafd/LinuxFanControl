// (c) 2025 LinuxFanControl contributors. MIT License.
using System.Threading.Tasks;
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
            // Pass the owning window into the ViewModel (required by its ctor)
            DataContext ??= new SetupDialogViewModel(this);
        }

        private void InitializeComponent() => AvaloniaXamlLoader.Load(this);

        // Convenience wrapper: await dlg.ShowAsync(owner)
        public Task ShowAsync(Window owner) => this.ShowDialog(owner);
    }
}
