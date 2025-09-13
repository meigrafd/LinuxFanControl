// (c) 2025 LinuxFanControl contributors. MIT License.
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using LiveChartsCore;
using LiveChartsCore.SkiaSharpView;

namespace LinuxFanControl.Gui.ViewModels
{
    public partial class FanTileViewModel : ObservableObject
    {
        [ObservableProperty] private string title;
        [ObservableProperty] private int rpm;
        [ObservableProperty] private int pwmDuty;
        [ObservableProperty] private string temperature;

        public ISeries[] Series { get; }

        public FanTileViewModel(string title, int rpm, int pwmDuty, string temperature)
        {
            this.title = title;
            this.rpm = rpm;
            this.pwmDuty = pwmDuty;
            this.temperature = temperature;

            // Simple sparkline series (dummy values)
            Series = new ISeries[]
            {
                new LineSeries<double> { Values = new double[] { 30, 32, 31, 33, 34, 35 } }
            };
        }

        [RelayCommand] private void Edit() { /* open editor dialog */ }
        [RelayCommand] private void Remove() { /* remove from dashboard */ }
    }
}
