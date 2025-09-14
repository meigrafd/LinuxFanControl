using Gtk;
using FanControl.Gui.Widgets;
using System.Collections.Generic;

namespace FanControl.Gui.Views;

public class ProfileView : Box
{
    private readonly CurveEditor _curveEditor;

    public ProfileView() : base(Orientation.Vertical, 10)
    {
        Margin = 20;

        var title = new Label(Translation.T("sidebar.profile"))
        {
            MarginBottom = 10
        };

        _curveEditor = new CurveEditor();
        _curveEditor.SetSizeRequest(600, 300);

        var saveButton = new Button("Speichern");
        saveButton.Clicked += (s, e) =>
        {
            var points = _curveEditor.GetPoints();
            // Hier kannst du die Punkte z.B. an den Daemon senden oder als JSON speichern
            System.Console.WriteLine("Kurve gespeichert:");
            foreach (var (x, y) in points)
                System.Console.WriteLine($"  {x:F1}°C → {y:F1}%");
        };

        PackStart(title, false, false, 0);
        PackStart(_curveEditor, true, true, 0);
        PackStart(saveButton, false, false, 0);
    }

    public void LoadCurve(List<(double x, double y)> points)
    {
        _curveEditor.SetPoints(points);
    }
}
