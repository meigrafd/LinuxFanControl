using Gtk;
using Cairo;
using System.Collections.Generic;
using FanControl.Gui.Services;

namespace FanControl.Gui.Widgets;

public class FanSpeedGraph : DrawingArea
{
    private readonly RpcClient _rpc;
    private readonly string _fanLabel;
    private readonly List<int> _rpmHistory = new();
    private const int MaxPoints = 100;

    public FanSpeedGraph(string fanLabel)
    {
        _fanLabel = fanLabel;
        _rpc = new RpcClient();

        SetSizeRequest(300, 100);
        Drawn += OnDraw;

        GLib.Timeout.Add(2000, () =>
        {
            UpdateRpm();
            QueueDraw();
            return true;
        });
    }

    private void UpdateRpm()
    {
        try
        {
            var response = _rpc.SendRequest("getFanSpeeds");
            if (response is System.Text.Json.Nodes.JsonObject obj && obj["result"] is System.Text.Json.Nodes.JsonArray fans)
            {
                foreach (var fan in fans)
                {
                    if (fan?["label"]?.ToString() == _fanLabel)
                    {
                        int rpm = fan["rpm"]?.GetValue<int>() ?? 0;
                        _rpmHistory.Add(rpm);
                        if (_rpmHistory.Count > MaxPoints)
                            _rpmHistory.RemoveAt(0);
                        break;
                    }
                }
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

        if (_rpmHistory.Count < 2)
            return;

        int max = _rpmHistory.Max();
        int min = _rpmHistory.Min();
        int range = max - min;
        if (range == 0) range = 1;

        double w = Allocation.Width;
        double h = Allocation.Height;
        double dx = w / (MaxPoints - 1);

        cr.SetSourceRGB(0.2, 0.6, 0.8);
        cr.SetLineWidth(1.5);

        for (int i = 0; i < _rpmHistory.Count; i++)
        {
            double x = i * dx;
            double y = h - ((_rpmHistory[i] - min) / (double)range * h);
            if (i == 0)
                cr.MoveTo(x, y);
            else
                cr.LineTo(x, y);
        }

        cr.Stroke();
    }
}
