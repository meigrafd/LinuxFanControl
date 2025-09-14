using Gtk;
using Cairo;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace FanControl.Gui.Views;

public class FanControlThermalMap : DrawingArea
{
    private readonly RpcClient _rpc;
    private List<(string label, double value)> _sensors = new();

    public FanControlThermalMap()
    {
        SetSizeRequest(500, 300);
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        LoadTemperatures();
        Drawn += OnDraw;
    }

    private void LoadTemperatures()
    {
        _sensors.Clear();

        var response = _rpc.SendRequest("getSensorTemperatures");
        if (response is JsonObject obj && obj["result"] is JsonArray sensors)
        {
            foreach (var s in sensors)
            {
                var label = s?["label"]?.ToString() ?? "?";
                var value = s?["value"]?.GetValue<double>() ?? 0.0;
                _sensors.Add((label, value));
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
        double spacing = w / (_sensors.Count + 1);
        double maxTemp = _sensors.Max(s => s.value);
        double minTemp = _sensors.Min(s => s.value);
        double range = maxTemp - minTemp;
        if (range == 0) range = 1;

        cr.SelectFontFace("Sans", FontSlant.Normal, FontWeight.Normal);
        cr.SetFontSize(10);

        for (int i = 0; i < _sensors.Count; i++)
        {
            var (label, value) = _sensors[i];
            double x = spacing * (i + 1);
            double intensity = (value - minTemp) / range;
            cr.SetSourceRGB(1.0, 1.0 - intensity, 1.0 - intensity);
            cr.Rectangle(x - 20, h / 2 - 20, 40, 40);
            cr.Fill();

            cr.SetSourceRGB(0.2, 0.2, 0.2);
            cr.MoveTo(x - 30, h / 2 + 30);
            cr.ShowText($"{label}: {value:F1}°C");
        }
    }

    private void Redraw()
    {
        LoadTemperatures();
    }
}
