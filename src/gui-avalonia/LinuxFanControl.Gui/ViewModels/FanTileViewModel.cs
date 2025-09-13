// (c) 2025 LinuxFanControl contributors. MIT License.
// Compact data for a single fan tile + sparkline series.

using System;
using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using LiveChartsCore;
using LiveChartsCore.SkiaSharpView;

namespace LinuxFanControl.Gui.ViewModels
{
    public partial class FanTileViewModel : ObservableObject
    {
        private const int HistoryCapacity = 60; // ~60s @ 1Hz
        private readonly ObservableCollection<double> _history = new();

        [ObservableProperty] private string id = "";
        [ObservableProperty] private string name = "";
        [ObservableProperty] private string sensor = "";
        [ObservableProperty] private string mode = "Auto";
        [ObservableProperty] private int dutyPercent;
        [ObservableProperty] private int rpm;
        [ObservableProperty] private double tempC;

        // sparkline series (temperature)
        public ISeries[] SparkSeries { get; }

        public FanTileViewModel(string id)
        {
            this.id = id;
            SparkSeries = new ISeries[]
            {
                new LineSeries<double>
                {
                    Values = _history,
                    GeometrySize = 0,
                    Fill = null,
                    LineSmoothness = 0
                }
            };
        }

        public void UpdateFrom(FanSnapshot f)
        {
            Name = f.Name;
            Sensor = f.Sensor;
            Mode = f.Mode;
            DutyPercent = f.DutyPercent;
            Rpm = f.Rpm;
            TempC = f.TempC;

            // Append to history (fixed capacity)
            _history.Add(f.TempC);
            while (_history.Count > HistoryCapacity)
                _history.RemoveAt(0);
        }
    }
}
