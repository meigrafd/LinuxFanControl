#nullable enable
using System.Collections.ObjectModel;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels
{
    public class DashboardViewModel
    {
        public ObservableCollection<FanModel> Fans => _telemetry.Fans;
        private readonly MockTelemetryService _telemetry;
        public DashboardViewModel(MockTelemetryService telemetry) { _telemetry = telemetry; }
    }
}
