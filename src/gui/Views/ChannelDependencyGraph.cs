using Gtk;
using Cairo;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace FanControl.Gui.Views;

public class ChannelDependencyGraph : DrawingArea
{
    private readonly RpcClient _rpc;
    private List<(string source, string target)> _edges = new();

    public ChannelDependencyGraph()
    {
        SetSizeRequest(500, 400);
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        LoadDependencies();
        Drawn += OnDraw;
    }

    private void LoadDependencies()
    {
        _edges.Clear();

        var response = _rpc.SendRequest("getChannelDependencies");
        if (response is JsonObject obj && obj["result"] is JsonArray deps)
        {
            foreach (var dep in deps)
            {
                var src = dep?["source"]?.ToString() ?? "?";
                var tgt = dep?["target"]?.ToString() ?? "?";
                _edges.Add((src, tgt));
            }

            QueueDraw();
        }
    }

    private void OnDraw(object o, DrawnArgs args)
    {
        var cr = args.Cr;
        cr.SetSourceRGB(1, 1, 1);
        cr.Paint();

        cr.SetSourceRGB(0.1, 0.1, 0.1);
        cr.SelectFontFace("Sans", FontSlant.Normal, FontWeight.Bold);
        cr.SetFontSize(12);

        double w = Allocation.Width;
        double h = Allocation.Height;
        double spacing = h / (_edges.Count + 1);

        for (int i = 0; i < _edges.Count; i++)
        {
            var (src, tgt) = _edges[i];
            double y = spacing * (i + 1);

            cr.MoveTo(40, y);
            cr.ShowText(src);

            cr.MoveTo(w - 140, y);
            cr.ShowText(tgt);

            cr.SetSourceRGB(0.3, 0.6, 0.3);
            cr.MoveTo(100, y);
            cr.LineTo(w - 160, y);
            cr.Stroke();
            cr.SetSourceRGB(0.1, 0.1, 0.1);
        }
    }

    private void Redraw()
    {
        LoadDependencies();
    }
}
