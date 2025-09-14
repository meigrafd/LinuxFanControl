using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Widgets;

public class CurveEditor : VBox
{
    private RpcClient _rpc;
    private string _channelName;
    private List<(double x, double y)> _points = new();

    private Button _saveButton;

    public CurveEditor(string channelName)
    {
        Spacing = 10;
        _channelName = channelName;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _saveButton = new Button(Translation.Get("curve.save"));
        _saveButton.Clicked += (_, _) => SaveCurve();
        PackStart(_saveButton, false, false, 0);

        // Hier könntest du später eine echte Kurven-Zeichnung einbauen
    }

    private void SaveCurve()
    {
        var pointsArray = new JsonArray();
        foreach (var (x, y) in _points)
        {
            pointsArray.Add(new JsonObject { ["x"] = x, ["y"] = y });
        }

        var payload = new JsonObject
        {
            ["name"] = _channelName,
            ["points"] = pointsArray
        };

        _rpc.SendRequest("setChannelCurve", payload);
    }

    private void Redraw()
    {
        BuildUi();
    }
}
