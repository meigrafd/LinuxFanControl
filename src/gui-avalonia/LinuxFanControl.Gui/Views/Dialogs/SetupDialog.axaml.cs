using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    public partial class SetupDialog : Window
    {
        public SetupDialog()
        {
            InitializeComponent();
        }

        // Explicit XAML load to avoid generator issues
        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }
    }
}
