using Gtk;
using Cairo;
using System.Collections.Generic;
using FanControl.Gui.Services;

namespace FanControl.Gui.Widgets;

public class SensorTrendPlot : DrawingArea
{
    private readonly RpcClient _rpc;
    private readonly string _sensorPath;
    private readonly List<double> _history = new();
    private const int MaxPoints = 100;

    public SensorTrendPlot(string sensorPath)
    {
        _sensorPath = sensorPath;
        _rpc = new RpcClient();

        SetSizeRequest(300, 100);
        Drawn += OnDraw;

        GLib.Timeout.Add(2000, () =>
        {
            UpdateValue();
            QueueDraw();
            return true;
        });
    }

    private void UpdateValue()
    {
        try
        {
            var param = new System.Text.Json.Nodes.JsonObject { ["path"] = _sensorPath };
            var response = _rpc.SendRequest("readSensor", param);
            if (response is System.Text.Json.Nodes.JsonObject obj && obj["result"] is var valNode && valNode != null)
            {
                double value = valNode.GetValue<double>();
                _history.Add(value);
                if (_history.Count > MaxPoints)
                    _history.RemoveAt(0);
            }
        }
        catch
        {
            // Ignorieren bei Fehler
        }
    }

    private void OnDraw(object o, DrawnArgs args)
    {
        var cr = args.Cr;
        cr.SetSourceRGB(1, 1, 1);
        cr.Paint();

        if (_history.Count < 2)
            return;

        double max = _history.Max();
        double min = _history.Min();
        double range = max - min;
        if (range == 0) range = 1;

        double w = Allocation.Width;
        double h = Allocation.Height;
        double dx = w / (MaxPoints - 1);

        cr.SetSourceRGB(0.8, 0.2, 0.2);
        cr.SetLineWidth(1.5);

        for (int i = 0; i < _history.Count; i++)
        {
            double x = i * dx;
            double y = h - ((_history[i] - min) / range * h);
            if (i == 0)
                cr.MoveTo(x, y);
            else
                cr.LineTo(x, y);
        }

        cr.Stroke();
    }
}
