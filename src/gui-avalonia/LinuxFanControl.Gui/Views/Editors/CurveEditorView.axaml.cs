// (c) 2025 LinuxFanControl contributors. MIT License.
// Drag & drop support over LiveCharts chart: map pointer to data (0..100).

using System;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Markup.Xaml;
using LiveChartsCore.Kernel.Sketches;
using LiveChartsCore.SkiaSharpView.Avalonia;
using LinuxFanControl.Gui.ViewModels;

namespace LinuxFanControl.Gui.Views.Editors
{
    public partial class CurveEditorView : UserControl
    {
        private bool _dragging;
        public CurveEditorView()
        {
            InitializeComponent();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }

        private void OnChartPointerPressed(object? sender, PointerPressedEventArgs e)
        {
            _dragging = true;
            UpdateDrag(sender, e);
        }

        private void OnChartPointerMoved(object? sender, PointerEventArgs e)
        {
            if (!_dragging) return;
            UpdateDrag(sender, e);
        }

        private void OnChartPointerReleased(object? sender, PointerReleasedEventArgs e)
        {
            _dragging = false;
        }

        private void UpdateDrag(object? sender, PointerEventArgs e)
        {
            if (DataContext is not CurveEditorViewModel vm) return;
            if (sender is not CartesianChart chart) return;

            try
            {
                // Use chart core's draw margin to map pixel->data
                var p = e.GetPosition(chart);
                var core = chart.CoreChart; // LiveCharts internal core chart
                var dm = core.DrawMarginLocation;
                var sz = core.DrawMarginSize;

                if (sz.Width <= 0 || sz.Height <= 0) return;

                double xMin = vm.XAxes[0].MinLimit ?? 0;
                double xMax = vm.XAxes[0].MaxLimit ?? 100;
                double yMin = vm.YAxes[0].MinLimit ?? 0;
                double yMax = vm.YAxes[0].MaxLimit ?? 100;

                // Clamp within draw margin
                var px = Math.Clamp(p.X, dm.X, dm.X + sz.Width);
                var py = Math.Clamp(p.Y, dm.Y, dm.Y + sz.Height);

                // Linear map (left->right, bottom->top)
                double x = xMin + (px - dm.X) / sz.Width * (xMax - xMin);
                double y = yMax - (py - dm.Y) / sz.Height * (yMax - yMin);

                // Left button -> move nearest; Right button -> remove; Ctrl+Left -> add point
                var props = e.GetCurrentPoint(chart).Properties;

                if (props.IsRightButtonPressed)
                {
                    vm.RemoveNearest(x, y);
                }
                else
                {
                    if (props.IsLeftButtonPressed && (e.KeyModifiers & KeyModifiers.Control) != 0)
                        vm.AddPoint(x, y);
                    else
                        vm.MoveNearest(x, y);
                }
            }
            catch
            {
                // Fail silently if core internals are unavailable.
            }
        }
    }
}
