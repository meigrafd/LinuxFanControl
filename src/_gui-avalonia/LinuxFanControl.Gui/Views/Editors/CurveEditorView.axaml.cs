// (c) 2025 LinuxFanControl contributors. MIT License.
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Markup.Xaml;
using LinuxFanControl.Gui.ViewModels;

namespace LinuxFanControl.Gui.Views.Editors
{
    public partial class CurveEditorView : UserControl
    {
        public CurveEditorView() { InitializeComponent(); }
        private void InitializeComponent() => AvaloniaXamlLoader.Load(this);

        // Simplified pointer handling; avoids LiveCharts IChart DrawMargin API usage.
        private void OnPointerPressed(object? sender, PointerPressedEventArgs e)
        {
            if (DataContext is not CurveEditorViewModel vm) return;
            var p = e.GetPosition(this);
            // naive normalization to [0..100] range
            var x = (p.X / Bounds.Width) * (vm.MaxX - vm.MinX) + vm.MinX;
            var y = (1.0 - (p.Y / Bounds.Height)) * (vm.MaxY - vm.MinY) + vm.MinY;
            vm.AddPointCommand.Execute((x, y));
        }
    }
}
