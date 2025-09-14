using Gtk;
using Cairo;
using System;
using System.Collections.Generic;

namespace FanControl.Gui.Widgets;

public class CurveEditor : DrawingArea
{
    private readonly List<PointD> _points = new();
    private const double PointRadius = 5;
    private int? _dragIndex = null;

    public CurveEditor()
    {
        AddEvents((int)(
            Gdk.EventMask.ButtonPressMask |
            Gdk.EventMask.PointerMotionMask |
            Gdk.EventMask.ButtonReleaseMask
        ));

        ButtonPressEvent += OnClick;
        MotionNotifyEvent += OnMotion;
        ButtonReleaseEvent += OnRelease;
        Drawn += OnDraw;

        // Beispielpunkte
        _points.Add(new PointD(20, 80));
        _points.Add(new PointD(60, 40));
        _points.Add(new PointD(100, 20));
    }

    private void OnClick(object o, ButtonPressEventArgs args)
    {
        double x = args.Event.X;
        double y = args.Event.Y;

        for (int i = 0; i < _points.Count; i++)
        {
            var p = _points[i];
            if (Distance(p.X, p.Y, x, y) < PointRadius * 2)
            {
                _dragIndex = i;
                return;
            }
        }

        _points.Add(new PointD(x, y));
        QueueDraw();
    }

    private void OnMotion(object o, MotionNotifyEventArgs args)
    {
        if (_dragIndex is null) return;

        _points[_dragIndex.Value] = new PointD(args.Event.X, args.Event.Y);
        QueueDraw();
    }

    private void OnRelease(object o, ButtonReleaseEventArgs args)
    {
        _dragIndex = null;
    }

    private void OnDraw(object o, DrawnArgs args)
    {
        var cr = args.Cr;
        cr.SetSourceRGB(0.1, 0.1, 0.1);
        cr.Paint();

        cr.SetSourceRGB(0.8, 0.8, 0.8);
        cr.LineWidth = 2;

        for (int i = 0; i < _points.Count - 1; i++)
        {
            cr.MoveTo(_points[i].X, _points[i].Y);
            cr.LineTo(_points[i + 1].X, _points[i + 1].Y);
        }
        cr.Stroke();

        foreach (var p in _points)
        {
            cr.Arc(p.X, p.Y, PointRadius, 0, 2 * Math.PI);
            cr.SetSourceRGB(0.2, 0.6, 1.0);
            cr.Fill();
        }
    }

    private static double Distance(double x1, double y1, double x2, double y2)
    {
        double dx = x1 - x2;
        double dy = y1 - y2;
        return Math.Sqrt(dx * dx + dy * dy);
    }

    public void ClearPoints()
    {
        _points.Clear();
        QueueDraw();
    }

    public void ResetPoints()
    {
        _points.Clear();
        _points.Add(new PointD(20, 80));
        _points.Add(new PointD(60, 40));
        _points.Add(new PointD(100, 20));
        QueueDraw();
    }

    public List<PointD> GetPoints() => new(_points);
}
