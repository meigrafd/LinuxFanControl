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
            // IMPORTANT: pass the Window to the VM constructor
            DataContext ??= new SetupDialogViewModel(this);
        }

        private void InitializeComponent() => AvaloniaXamlLoader.Load(this);

        // Helper for code that expects dialog.ShowAsync(owner)
        public Task ShowAsync(Window owner) => this.ShowDialog(owner);
    }
}
