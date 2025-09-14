using Gtk;
using System;
using System.Collections.Generic;

namespace FanControl.Gui.Views;

public class CurveView : Box
{
    private readonly Entry _pointsEntry;
    private readonly Button _saveButton;
    private readonly Label _statusLabel;

    public CurveView() : base(Orientation.Vertical, 10)
    {
        Margin = 20;

        _pointsEntry = new Entry
        {
            PlaceholderText = "(30,40);(50,60);(70,80)"
        };

        _saveButton = new Button(Translation.T("curve.save"));
        _statusLabel = new Label("");

        _saveButton.Clicked += (_, _) => SaveCurve();

        PackStart(new Label(Translation.T("curve.title")), false, false, 0);
        PackStart(_pointsEntry, false, false, 0);
        PackStart(_saveButton, false, false, 0);
        PackStart(_statusLabel, false, false, 0);
    }

    private void SaveCurve()
    {
        try
        {
            var text = _pointsEntry.Text;
            var parts = text.Split(';');
            var curve = new List<(double x, double y)>();

            foreach (var part in parts)
            {
                var trimmed = part.Trim('(', ')');
                var xy = trimmed.Split(',');
                if (xy.Length != 2) continue;

                double x = double.Parse(xy[0]);
                double y = double.Parse(xy[1]);
                curve.Add((x, y));
            }

            // Hier würde die Engine angebunden werden
            _statusLabel.Text = "Curve saved.";
        }
        catch
        {
            _statusLabel.Text = "Invalid format.";
        }
    }
}
