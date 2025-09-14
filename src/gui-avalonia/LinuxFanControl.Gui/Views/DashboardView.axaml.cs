#nullable enable
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using LinuxFanControl.Gui.Services;
using LinuxFanControl.Gui.ViewModels;

namespace LinuxFanControl.Gui.Views
{
    public partial class DashboardView : UserControl
    {
        private readonly MockTelemetryService _telemetry = new();
        public DashboardView()
        {
            InitializeComponent();
            DataContext = new DashboardViewModel(_telemetry);
            this.AttachedToVisualTree += (_, __) => _ = _telemetry; // keep alive
        }
        private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
    }
}
