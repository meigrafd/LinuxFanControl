// (c) 2025 LinuxFanControl contributors. MIT License.
// Curve editor VM (XY points 0..100 / 0..100) with LiveCharts (XY).

using System;
using System.Collections.ObjectModel;
using System.Linq;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using LinuxFanControl.Gui.Services;
using LiveChartsCore;
using LiveChartsCore.SkiaSharpView;
using LiveChartsCore.Defaults;

namespace LinuxFanControl.Gui.ViewModels
{
    public partial class CurveEditorViewModel : ObservableObject, IEditorViewModel
    {
        public string FanId { get; }
        [ObservableProperty] private string title;
        [ObservableProperty] private bool isDirty;

        // XY points as ObservablePoint -> native XY in LiveCharts
        public ObservableCollection<ObservablePoint> Points { get; } = new();

        [ObservableProperty] private ISeries[] series = Array.Empty<ISeries>();
        public Axis[] XAxes { get; } = { new Axis { Name = "°C", MinLimit = 0, MaxLimit = 100 } };
        public Axis[] YAxes { get; } = { new Axis { Name = "%", MinLimit = 0, MaxLimit = 100 } };

        private readonly IDaemonClient _client;

        public event EventHandler? CloseRequested;

        public CurveEditorViewModel(string fanId, string fanName, IDaemonClient client)
        {
            FanId = fanId;
            title = $"{fanName} • Curve";
            _client = client;

            // Default curve
            Points.Add(new ObservablePoint(20, 20));
            Points.Add(new ObservablePoint(40, 40));
            Points.Add(new ObservablePoint(60, 70));
            Points.Add(new ObservablePoint(80, 100));

            RebuildSeries();
        }

        private void RebuildSeries()
        {
            // Keep points sorted by X
            var ordered = Points.OrderBy(p => p.X).ToList();
            Points.Clear();
            foreach (var p in ordered) Points.Add(p);

            Series = new ISeries[]
            {
                new LineSeries<ObservablePoint>
                {
                    Values = Points,
                    GeometrySize = 0,
                    Fill = null,
                    LineSmoothness = 0
                },
                new ScatterSeries<ObservablePoint>
                {
                    Values = Points,
                    GeometrySize = 11
                }
            };
        }

        [RelayCommand]
        private async void Save()
        {
            var pts = Points.Select(p => new CurvePoint { X = p.X, Y = p.Y }).ToArray();
            var ok = await _client.SetCurveAsync(FanId, pts);
            if (ok) IsDirty = false;
        }

        [RelayCommand]
        private void Close() => CloseRequested?.Invoke(this, EventArgs.Empty);

        public void MoveNearest(double x, double y)
        {
            if (Points.Count == 0) return;
            // Find nearest by Euclidean distance in data space
            int idx = 0; double best = double.MaxValue;
            for (int i = 0; i < Points.Count; i++)
            {
                var dx = Points[i].X - x;
                var dy = Points[i].Y - y;
                var d2 = dx * dx + dy * dy;
                if (d2 < best) { best = d2; idx = i; }
            }
            // Constrain X monotonic between neighbors
            double left = (idx > 0) ? Points[idx - 1].X + 0.1 : 0.0;
            double right = (idx < Points.Count - 1) ? Points[idx + 1].X - 0.1 : 100.0;
            var nx = Math.Clamp(x, left, right);
            var ny = Math.Clamp(y, 0.0, 100.0);

            Points[idx].X = nx;
            Points[idx].Y = ny;
            IsDirty = true;
        }

        public void AddPoint(double x, double y)
        {
            Points.Add(new ObservablePoint(Math.Clamp(x,0,100), Math.Clamp(y,0,100)));
            RebuildSeries();
            IsDirty = true;
        }

        public void RemoveNearest(double x, double y)
        {
            if (Points.Count <= 2) return;
            int idx = 0; double best = double.MaxValue;
            for (int i = 0; i < Points.Count; i++)
            {
                var dx = Points[i].X - x;
                var dy = Points[i].Y - y;
                var d2 = dx * dx + dy * dy;
                if (d2 < best) { best = d2; idx = i; }
            }
            Points.RemoveAt(idx);
            IsDirty = true;
        }
    }
}
