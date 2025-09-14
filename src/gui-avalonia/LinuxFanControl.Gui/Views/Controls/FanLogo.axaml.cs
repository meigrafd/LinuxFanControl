using System;
using Avalonia;
using Avalonia.Controls.Shapes;
using Avalonia.Media;

namespace LinuxFanControl.Gui.Views.Controls
{
    public class FanLogo : Shape
    {
        public static readonly StyledProperty<double> AngleProperty =
        AvaloniaProperty.Register<FanLogo, double>(nameof(Angle));

        public double Angle
        {
            get => GetValue(AngleProperty);
            set => SetValue(AngleProperty, value);
        }

        public override void Render(DrawingContext context)
        {
            base.Render(context);

            // Berechne Kreis-Mitte und Radius
            var center = new RelativePoint(Bounds.Center, RelativeUnit.Absolute);
            var radius = Math.Min(Bounds.Width, Bounds.Height) / 2;

            // Erzeuge eine einfache Linien-Geometrie als Platzhalter
            var geometry = new StreamGeometry();
            using (var ctx = geometry.Open())
            {
                ctx.BeginFigure(
                    new Point(center.Point.X + radius, center.Point.Y),
                                isFilled: false);
                ctx.LineTo(new Point(center.Point.X, center.Point.Y - radius));
                ctx.LineTo(new Point(center.Point.X - radius, center.Point.Y));
                ctx.EndFigure(isClosed: false);
            }

            // Zeichne die Geometrie
            context.DrawGeometry(
                Brushes.LightGray,
                new Pen(Brushes.DarkGray, 2),
                                 geometry);
        }
    }
}
