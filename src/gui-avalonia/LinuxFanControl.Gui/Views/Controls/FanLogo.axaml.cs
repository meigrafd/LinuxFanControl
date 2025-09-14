// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using Avalonia;
using Avalonia.Controls.Shapes;
using Avalonia.Media;

namespace LinuxFanControl.Gui.Views.Controls
{
    public partial class FanLogo : Shape
    {
        public static readonly StyledProperty<double> AngleProperty =
        AvaloniaProperty.Register<FanLogo, double>(
            nameof(Angle),
                                                   0d);

        public double Angle
        {
            get => GetValue(AngleProperty);
            set => SetValue(AngleProperty, value);
        }

        protected override Geometry CreateDefiningGeometry()
        {
            var w = Bounds.Width;
            var h = Bounds.Height;
            var center = new Point(w / 2, h / 2);
            var radius = Math.Min(w, h) / 2;

            var geometry = new StreamGeometry();
            using (var ctx = geometry.Open())
            {
                ctx.BeginFigure(
                    new Point(center.X + radius, center.Y),
                                isFilled: false);

                ctx.LineTo(new Point(center.X, center.Y - radius));
                ctx.LineTo(new Point(center.X - radius, center.Y));
                ctx.EndFigure(isClosed: false);
            }

            return geometry;
        }
    }
}
