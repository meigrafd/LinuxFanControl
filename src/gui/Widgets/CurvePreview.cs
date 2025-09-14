using Gtk;
using Cairo;
using System.Collections.Generic;

namespace FanControl.Gui.Widgets;

public class CurvePreview : DrawingArea
{
    private List<(double x, double y)> _points = new();

    public CurvePreview()
    {
        SetSizeRequest(300, 200);
        Drawn += OnDraw;
    }

    public void SetPoints(List<(double x, double y)> points)
    {
        _points = new(points);
        QueueDraw();
    }

    private void OnDraw(object sender, DrawnArgs args)
    {
        var cr = args.Cr;
        cr.SetSourceRGB(1, 1, 1);
        cr.Paint();

        if (_points.Count < 2)
            return;

        cr.SetSourceRGB(0.2, 0.6, 0.8);
        cr.LineWidth = 2;

        double width = Allocation.Width;
        double height = Allocation.Height;

        double xMin = _points[0].x;
        double xMax = _points[^1].x;
        double yMin = 0;
        double yMax = 100;

        for (int i = 0; i < _points.Count; i++)
        {
            double x = (_points[i].x - xMin) / (xMax - xMin) * width;
            double y = height - (_points[i].y - yMin) / (yMax - yMin) * height;

            if (i == 0)
                cr.MoveTo(x, y);
            else
                cr.LineTo(x, y);
        }

        cr.Stroke();
    }
}
