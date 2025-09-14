using Gtk;
using Cairo;
using Gdk;
using System.Collections.Generic;
using System.Linq;

namespace FanControl.Gui.Widgets;

public class CurveEditor : DrawingArea
{
    private List<(double x, double y)> _points = new()
    {
        (30, 20),
        (50, 50),
        (70, 100)
    };

    private const int PointRadius = 6;
    private int? _dragIndex = null;

    public CurveEditor()
    {
        AddEvents((int)(
            EventMask.ButtonPressMask |
            EventMask.ButtonReleaseMask |
            EventMask.PointerMotionMask
        ));

        ButtonPressEvent += OnButtonPress;
        ButtonReleaseEvent += OnButtonRelease;
        MotionNotifyEvent += OnMotion;
        Drawn += OnDraw;
    }

    private void OnDraw(object o, DrawnArgs args)
    {
        var cr = args.Cr;
        cr.SetSourceRGB(0.1, 0.1, 0.1);
        cr.Paint();

        cr.SetSourceRGB(0.8, 0.8, 0.8);
        cr.LineWidth = 2;

        var sorted = _points.OrderBy(p => p.x).ToList();
        for (int i = 0; i < sorted.Count - 1; i++)
        {
            cr.MoveTo(ToScreenX(sorted[i].x), ToScreenY(sorted[i].y));
            cr.LineTo(ToScreenX(sorted[i + 1].x), ToScreenY(sorted[i + 1].y));
        }
        cr.Stroke();

        foreach (var (x, y) in sorted)
        {
            cr.Arc(ToScreenX(x), ToScreenY(y), PointRadius, 0, 2 * System.Math.PI);
            cr.SetSourceRGB(0.2, 0.6, 1.0);
            cr.Fill();
        }
    }

    private void OnButtonPress(object o, ButtonPressEventArgs args)
    {
        double mx = args.Event.X;
        double my = args.Event.Y;

        for (int i = 0; i < _points.Count; i++)
        {
            var (x, y) = _points[i];
            double dx = ToScreenX(x) - mx;
            double dy = ToScreenY(y) - my;
            if (dx * dx + dy * dy < PointRadius * PointRadius * 2)
            {
                if (args.Event.Button == 3) // Rechtsklick
                {
                    _points.RemoveAt(i);
                    QueueDraw();
                    return;
                }

                _dragIndex = i;
                return;
            }
        }

        if (args.Event.Button == 1) // Linksklick → neuen Punkt hinzufügen
        {
            double tx = FromScreenX(mx);
            double ty = FromScreenY(my);
            _points.Add((tx, ty));
            QueueDraw();
        }
    }

    private void OnButtonRelease(object o, ButtonReleaseEventArgs args)
    {
        _dragIndex = null;
    }

    private void OnMotion(object o, MotionNotifyEventArgs args)
    {
        if (_dragIndex is null) return;

        double tx = FromScreenX(args.Event.X);
        double ty = FromScreenY(args.Event.Y);
        _points[_dragIndex.Value] = (tx, ty);
        QueueDraw();
    }

    private double ToScreenX(double x) => x / 100.0 * Allocation.Width;
    private double ToScreenY(double y) => Allocation.Height - y / 100.0 * Allocation.Height;
    private double FromScreenX(double sx) => sx / Allocation.Width * 100.0;
    private double FromScreenY(double sy) => (Allocation.Height - sy) / Allocation.Height * 100.0;

    public List<(double x, double y)> GetPoints() => _points.OrderBy(p => p.x).ToList();
    public void SetPoints(List<(double x, double y)> points)
    {
        _points = points;
        QueueDraw();
    }
}
