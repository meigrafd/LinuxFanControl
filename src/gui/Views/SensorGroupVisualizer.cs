using Gtk;
using Cairo;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace FanControl.Gui.Views;

public class SensorGroupVisualizer : DrawingArea
{
    private readonly RpcClient _rpc;
    private List<(string group, List<string> members)> _groups = new();

    public SensorGroupVisualizer()
    {
        SetSizeRequest(600, 400);
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        LoadGroups();
        Drawn += OnDraw;
    }

    private void LoadGroups()
    {
        _groups.Clear();

        var response = _rpc.SendRequest("getSensorGroups");
        if (response is JsonObject obj && obj["result"] is JsonArray groups)
        {
            foreach (var g in groups)
            {
                var name = g?["name"]?.ToString() ?? "?";
                var members = new List<string>();
                if (g?["members"] is JsonArray m)
                {
                    foreach (var s in m)
                        members.Add(s?.ToString() ?? "?");
                }
                _groups.Add((name, members));
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
        cr.SelectFontFace("Sans", FontSlant.Normal, FontWeight.Bold);
        cr.SetFontSize(12);

        double x = 50;
        double y = 40;

        foreach (var (group, members) in _groups)
        {
            cr.MoveTo(x, y);
            cr.ShowText($"Group: {group}");
            y += 20;

            cr.SetFontSize(10);
            foreach (var m in members)
            {
                cr.MoveTo(x + 20, y);
                cr.ShowText($"• {m}");
                y += 15;
            }

            y += 20;
            cr.SetFontSize(12);
        }
    }

    private void Redraw()
    {
        LoadGroups();
    }
}
