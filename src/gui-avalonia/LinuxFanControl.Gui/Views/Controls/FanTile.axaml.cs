using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace LinuxFanControl.Gui.Views.Controls
{
    /// <summary>
    /// Dashboard fan tile. Binds to FanTileViewModel (Name, SensorDisplay, RpmDisplay, Duty).
    /// </summary>
    public partial class FanTile : UserControl
    {
        public FanTile()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }
    }
}
