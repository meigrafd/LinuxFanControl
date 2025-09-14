using Gtk;
using Cairo;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace FanControl.Gui.Views;

public class SensorHistoryExplorer : DrawingArea
{
    private readonly RpcClient _rpc;
    private readonly string _sensorPath;
    private List<(DateTime ts, double value)> _history = new();

    public SensorHistoryExplorer(string sensorPath)
    {
        _sensorPath = sensorPath;
        _rpc = new RpcClient();

        SetSizeRequest(400, 200);
        Translation.LanguageChanged += Redraw;

        LoadHistory();
        Drawn += OnDraw;
    }

    private void LoadHistory()
    {
        _history.Clear();

        var payload = new JsonObject { ["path"] = _sensorPath };
        var response = _rpc.SendRequest("getSensorHistory", payload);
        if (response is JsonObject obj && obj["result"] is JsonArray entries)
        {
            foreach (var e in entries)
            {
                var ts = DateTime.Parse(e?["timestamp"]?.ToString() ?? "");
                var val = e?["value"]?.GetValue<double>() ?? 0.0;
                _history.Add((ts, val));
            }

            QueueDraw();
        }
    }

    private void OnDraw(object o, DrawnArgs args)
    {
        var cr = args.Cr;
        cr.SetSourceRGB(1, 1, 1);
        cr.Paint();

        if (_history.Count < 2)
            return;

        double w = Allocation.Width;
        double h = Allocation.Height;

        double min = _history.Min(p => p.value);
        double max = _history.Max(p => p.value);
        double range = max - min;
        if (range == 0) range = 1;

        double dx = w / (_history.Count - 1);

        cr.SetSourceRGB(0.8, 0.3, 0.3);
        cr.SetLineWidth(1.5);

        for (int i = 0; i < _history.Count; i++)
        {
            double x = i * dx;
            double y = h - ((_history[i].value - min) / range * h);
            if (i == 0)
                cr.MoveTo(x, y);
            else
                cr.LineTo(x, y);
        }

        cr.Stroke();
    }

    private void Redraw()
    {
        LoadHistory();
    }
}
