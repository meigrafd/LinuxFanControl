using Gtk;
using Cairo;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace FanControl.Gui.Views;

public class SensorCorrelationMatrix : DrawingArea
{
    private readonly RpcClient _rpc;
    private List<string> _labels = new();
    private double[,] _matrix;

    public SensorCorrelationMatrix()
    {
        SetSizeRequest(400, 400);
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        LoadMatrix();
        Drawn += OnDraw;
    }

    private void LoadMatrix()
    {
        var response = _rpc.SendRequest("getSensorCorrelations");
        if (response is JsonObject obj && obj["result"] is JsonObject result)
        {
            if (result["labels"] is JsonArray labelArr && result["matrix"] is JsonArray rows)
            {
                _labels.Clear();
                foreach (var l in labelArr)
                    _labels.Add(l?.ToString() ?? "?");

                int n = _labels.Count;
                _matrix = new double[n, n];

                for (int i = 0; i < n; i++)
                {
                    if (rows[i] is JsonArray row)
                    {
                        for (int j = 0; j < n; j++)
                            _matrix[i, j] = row[j]?.GetValue<double>() ?? 0.0;
                    }
                }

                QueueDraw();
            }
        }
    }

    private void OnDraw(object o, DrawnArgs args)
    {
        var cr = args.Cr;
        cr.SetSourceRGB(1, 1, 1);
        cr.Paint();

        if (_matrix == null || _labels.Count == 0)
            return;

        int n = _labels.Count;
        double w = Allocation.Width;
        double h = Allocation.Height;
        double cellSize = Math.Min(w, h) / (n + 1);

        cr.SelectFontFace("Sans", FontSlant.Normal, FontWeight.Normal);
        cr.SetFontSize(10);

        for (int i = 0; i < n; i++)
        {
            cr.MoveTo((i + 1) * cellSize, cellSize * 0.8);
            cr.ShowText(_labels[i]);

            cr.MoveTo(cellSize * 0.2, (i + 1) * cellSize);
            cr.ShowText(_labels[i]);
        }

        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                double val = _matrix[i, j];
                double intensity = Math.Abs(val);
                cr.SetSourceRGB(1 - intensity, 1 - intensity, 1.0);
                cr.Rectangle((j + 1) * cellSize, (i + 1) * cellSize, cellSize * 0.9, cellSize * 0.9);
                cr.Fill();
            }
        }
    }

    private void Redraw()
    {
        LoadMatrix();
    }
}
