using Gtk;
using Cairo;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Widgets;

public class ChannelCurvePlot : DrawingArea
{
    private readonly RpcClient _rpc;
    private readonly string _channelName;
    private List<(double x, double y)> _points = new();

    public ChannelCurvePlot(string channelName)
    {
        _channelName = channelName;
        _rpc = new RpcClient();

        SetSizeRequest(300, 150);
        Translation.LanguageChanged += Redraw;

        LoadCurve();
        Drawn += OnDraw;
    }

    private void LoadCurve()
    {
        var response = _rpc.SendRequest("listChannels");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            foreach (var ch in channels)
            {
                if (ch?["name"]?.ToString() == _channelName && ch["points"] is JsonArray pts)
                {
                    _points.Clear();
                    foreach (var pt in pts)
                    {
                        double x = pt?["x"]?.GetValue<double>() ?? 0.0;
                        double y = pt?["y"]?.GetValue<double>() ?? 0.0;
                        _points.Add((x, y));
                    }
                    QueueDraw();
                    break;
                }
            }
        }
    }

    private void OnDraw(object o, DrawnArgs args)
    {
        var cr = args.Cr;
        cr.SetSourceRGB(1, 1, 1);
        cr.Paint();

        cr.SetSourceRGB(0.2, 0.2, 0.8);
        cr.SetLineWidth(2);

        if (_points.Count > 1)
        {
            var w = Allocation.Width;
            var h = Allocation.Height;

            double maxX = _points.Max(p => p.x);
            double maxY = _points.Max(p => p.y);

            cr.MoveTo(_points[0].x / maxX * w, h - (_points[0].y / maxY * h));
            for (int i = 1; i < _points.Count; i++)
            {
                double px = _points[i].x / maxX * w;
                double py = h - (_points[i].y / maxY * h);
                cr.LineTo(px, py);
            }
            cr.Stroke();
        }
    }

    private void Redraw()
    {
        LoadCurve();
    }
}
