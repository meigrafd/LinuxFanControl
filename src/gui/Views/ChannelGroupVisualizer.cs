using Gtk;
using Cairo;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace FanControl.Gui.Views;

public class ChannelGroupVisualizer : DrawingArea
{
    private readonly RpcClient _rpc;
    private List<(string name, string output)> _channels = new();

    public ChannelGroupVisualizer()
    {
        SetSizeRequest(400, 300);
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        LoadChannels();
        Drawn += OnDraw;
    }

    private void LoadChannels()
    {
        _channels.Clear();

        var response = _rpc.SendRequest("listChannels");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            foreach (var ch in channels)
            {
                var name = ch?["name"]?.ToString() ?? "?";
                var output = ch?["output_label"]?.ToString() ?? "?";
                _channels.Add((name, output));
            }

            QueueDraw();
        }
    }

    private void OnDraw(object o, DrawnArgs args)
    {
        var cr = args.Cr;
        cr.SetSourceRGB(1, 1, 1);
        cr.Paint();

        cr.SetSourceRGB(0.2, 0.2, 0.2);
        cr.SelectFontFace("Sans", FontSlant.Normal, FontWeight.Normal);
        cr.SetFontSize(12);

        double w = Allocation.Width;
        double h = Allocation.Height;
        double spacing = h / (_channels.Count + 1);

        for (int i = 0; i < _channels.Count; i++)
        {
            var (name, output) = _channels[i];
            double y = spacing * (i + 1);

            cr.MoveTo(20, y);
            cr.ShowText(name);

            cr.MoveTo(w - 120, y);
            cr.ShowText(output);

            cr.SetSourceRGB(0.4, 0.4, 0.8);
            cr.MoveTo(100, y);
            cr.LineTo(w - 140, y);
            cr.Stroke();
            cr.SetSourceRGB(0.2, 0.2, 0.2);
        }
    }

    private void Redraw()
    {
        LoadChannels();
    }
}
