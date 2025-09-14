// (c) 2025 LinuxFanControl contributors. MIT License.

using ReactiveUI;

namespace LinuxFanControl.Gui.ViewModels
{
    public sealed class FanTileViewModel : ReactiveObject
    {
        public string Id { get; set; } = System.Guid.NewGuid().ToString("N");
        private string _name = "Fan";
        public string Name { get => _name; set => this.RaiseAndSetIfChanged(ref _name, value); }

        private string _sensorLabel = "--";
        public string SensorLabel { get => _sensorLabel; set => this.RaiseAndSetIfChanged(ref _sensorLabel, value); }

        private int _duty = 0;
        public int Duty { get => _duty; set => this.RaiseAndSetIfChanged(ref _duty, value); }

        private int _rpm = 0;
        public int Rpm { get => _rpm; set => this.RaiseAndSetIfChanged(ref _rpm, value); }
    }
}
