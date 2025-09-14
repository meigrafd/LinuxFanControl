using Gtk;
using Cairo;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace FanControl.Gui.Tools;

public class FanControlRuleDependencyGraph : DrawingArea
{
    private readonly RpcClient _rpc;
    private List<(string source, string target)> _edges = new();

    public FanControlRuleDependencyGraph()
    {
        SetSizeRequest(600, 400);
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        LoadDependencies();
        Drawn += OnDraw;
    }

    private void LoadDependencies()
    {
        _edges.Clear();

        var response = _rpc.SendRequest("getRuleDependencyGraph");
        if (response is JsonObject obj && obj["result"] is JsonArray edges)
        {
            foreach (var e in edges)
            {
                var src = e?["source"]?.ToString() ?? "?";
                var tgt = e?["target"]?.ToString() ?? "?";
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

        cr.SetSourceRGB(0.2, 0.2, 0.2);
        cr.SelectFontFace("Sans", FontSlant.Normal, FontWeight.Normal);
        cr.SetFontSize(10);

        double x = 50;
        double y = 50;
        double spacing = 80;

        var positions = new Dictionary<string, (double x, double y)>();
        int i = 0;

        foreach (var (src, tgt) in _edges)
        {
            if (!positions.ContainsKey(src))
                positions[src] = (x + (i * spacing), y);
            if (!positions.ContainsKey(tgt))
                positions[tgt] = (x + (i * spacing), y + spacing);
            i++;
        }

        foreach (var (src, tgt) in _edges)
        {
            var (x1, y1) = positions[src];
            var (x2, y2) = positions[tgt];

            cr.MoveTo(x1, y1);
            cr.LineTo(x2, y2);
            cr.Stroke();

            cr.MoveTo(x1 - 10, y1 - 5);
            cr.ShowText(src);

            cr.MoveTo(x2 - 10, y2 - 5);
            cr.ShowText(tgt);
        }
    }

    private void Redraw()
    {
        LoadDependencies();
    }
}
