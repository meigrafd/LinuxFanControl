// (c) 2025 LinuxFanControl contributors. MIT License.
#nullable enable
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Shapes;

namespace LinuxFanControl.Gui.Views.Controls
{
    public partial class FanLogo : UserControl
    {
        public FanLogo()
        {
            var canvas = new Canvas
            {
                Width = 64,
                Height = 64
            };

            var ellipse = new Ellipse
            {
                Width = 64,
                Height = 64,
                Fill = TryFindResource("Lfc.Accent", out var accent) ? (IBrush?)accent : Brushes.Blue
            };

            var path = new Path
            {
                Stroke = TryFindResource("Lfc.WindowBg", out var stroke) ? (IBrush?)stroke : Brushes.Black,
                StrokeThickness = 4,
                Data = Geometry.Parse("M32,0 L32,64 M0,32 L64,32")
            };

            canvas.Children.Add(ellipse);
            canvas.Children.Add(path);
            Content = canvas;
        }
    }
}
