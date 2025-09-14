using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace LinuxFanControl.Gui.Views.Controls
{
    public partial class FanTile : UserControl
    {
        public FanTile() { InitializeComponent(); }
        private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
    }
}
