// (c) 2025 LinuxFanControl contributors. MIT License.

using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace LinuxFanControl.Gui.Views
{
    public partial class DashboardView : UserControl
    {
        public DashboardView()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }
    }
}
