// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace LinuxFanControl.Gui.ViewModels
{
    public sealed partial class CurveEditorViewModel : ObservableObject
    {
        public ObservableCollection<Point> Points { get; } = new();

        [ObservableProperty] private double minX = 0;
        [ObservableProperty] private double maxX = 100;
        [ObservableProperty] private double minY = 0;
        [ObservableProperty] private double maxY = 100;

        public CurveEditorViewModel()
        {
            Points.Add(new Point(20, 20));
            Points.Add(new Point(40, 40));
            Points.Add(new Point(60, 60));
            Points.Add(new Point(80, 100));
        }

        // RelayCommand must take a single parameter. We accept a ValueTuple (x,y).
        [RelayCommand]
        public void AddPoint((double x, double y) p)
        {
            var (x, y) = p;
            x = Math.Clamp(x, MinX, MaxX);
            y = Math.Clamp(y, MinY, MaxY);
            Points.Add(new Point(x, y));
        }

        [RelayCommand]
        public void Save()
        {
            // Persist curve to config or send to daemon.
        }

        public readonly record struct Point(double X, double Y);
    }
}
