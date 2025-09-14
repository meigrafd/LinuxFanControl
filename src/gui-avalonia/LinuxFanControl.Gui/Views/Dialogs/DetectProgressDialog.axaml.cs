#nullable enable
using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    public partial class DetectProgressDialog : Window
    {
        public DetectProgressDialog() { InitializeComponent(); }
        private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
    }
}
