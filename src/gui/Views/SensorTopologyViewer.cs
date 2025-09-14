using Gtk;
using Cairo;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace FanControl.Gui.Views;

public class SensorTopologyViewer : DrawingArea
{
    private readonly RpcClient _rpc;
    private List<(string parent, string child)> _edges = new();
    private HashSet<string> _nodes = new();

    public SensorTopologyViewer()
    {
        SetSizeRequest(500, 400);
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        LoadTopology();
        Drawn += OnDraw;
    }

    private void LoadTopology()
    {
        _edges.Clear();
        _nodes.Clear();

        var response = _rpc.SendRequest("getSensorTopology");
        if (response is JsonObject obj && obj["result"] is JsonArray links)
        {
            foreach (var link in links)
            {
                var parent = link?["parent"]?.ToString() ?? "?";
                var child = link?["child"]?.ToString() ?? "?";
                _edges.Add((parent, child));
                _nodes.Add(parent);
                _nodes.Add(child);
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
        cr.SetFontSize(11);

        double w = Allocation.Width;
        double h = Allocation.Height;
        double radius = 6;
        double spacing = h / (_nodes.Count + 1);

        var positions = new Dictionary<string, (double x, double y)>();
        int i = 0;
        foreach (var node in _nodes)
        {
            double y = spacing * (i + 1);
            positions[node] = (w / 2, y);
            cr.MoveTo(w / 2 + 10, y);
            cr.ShowText(node);
            cr.Arc(w / 2, y, radius, 0, 2 * Math.PI);
            cr.Fill();
            i++;
        }

        cr.SetSourceRGB(0.4, 0.4, 0.8);
        foreach (var (parent, child) in _edges)
        {
            if (positions.ContainsKey(parent) && positions.ContainsKey(child))
            {
                var (x1, y1) = positions[parent];
                var (x2, y2) = positions[child];
                cr.MoveTo(x1, y1);
                cr.LineTo(x2, y2);
                cr.Stroke();
            }
        }
    }

    private void Redraw()
    {
        LoadTopology();
    }
}
