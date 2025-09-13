// (c) 2025 LinuxFanControl contributors. MIT License.

using Avalonia.Controls;

namespace LinuxFanControl.Gui.Views.Controls
{
    public partial class FanTile : UserControl
    {
        public FanTile()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            Avalonia.Markup.Xaml.AvaloniaXamlLoader.Load(this);
        }
    }
}
