#nullable enable
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels
{
    public class FanTileViewModel
    {
        public FanModel Model { get; }
        public FanTileViewModel(FanModel m) { Model = m; }
        public string Name => Model.Name;
        public double Duty { get => Model.Duty; set => Model.Duty = value; }
        public double Rpm => Model.Rpm;
        public double Temp => Model.Temp;
    }
}
