using Gtk;
using Cairo;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace FanControl.Gui.Views;

public class FanControlStateVisualizer : DrawingArea
{
    private readonly RpcClient _rpc;
    private List<(string label, double pwm)> _channels = new();

    public FanControlStateVisualizer()
    {
        SetSizeRequest(500, 300);
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        LoadState();
        Drawn += OnDraw;
    }

    private void LoadState()
    {
        _channels.Clear();

        var response = _rpc.SendRequest("getChannelStates");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            foreach (var ch in channels)
            {
                var label = ch?["name"]?.ToString() ?? "?";
                var pwm = ch?["pwm"]?.GetValue<double>() ?? 0.0;
                _channels.Add((label, pwm));
            }

            QueueDraw();
        }
    }

    private void OnDraw(object o, DrawnArgs args)
    {
        var cr = args.Cr;
        cr.SetSourceRGB(1, 1, 1);
        cr.Paint();

        double w = Allocation.Width;
        double h = Allocation.Height;
        double spacing = w / (_channels.Count + 1);

        cr.SelectFontFace("Sans", FontSlant.Normal, FontWeight.Normal);
        cr.SetFontSize(10);

        for (int i = 0; i < _channels.Count; i++)
        {
            var (label, pwm) = _channels[i];
            double x = spacing * (i + 1);
            double barHeight = pwm / 100.0 * h * 0.6;

            cr.SetSourceRGB(0.3, 0.6, 0.9);
            cr.Rectangle(x - 15, h - barHeight - 40, 30, barHeight);
            cr.Fill();

            cr.SetSourceRGB(0.2, 0.2, 0.2);
            cr.MoveTo(x - 30, h - 20);
            cr.ShowText($"{label}: {pwm:F0}%");
        }
    }

    private void Redraw()
    {
        LoadState();
    }
}
