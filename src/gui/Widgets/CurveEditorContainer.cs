using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Widgets;

public class CurveEditorContainer : VBox
{
    private RpcClient _rpc;
    private string _channelName;
    private List<(double x, double y)> _points = new();

    private Button _saveButton;
    private Button _resetButton;
    private Button _clearButton;

    public CurveEditorContainer(string channelName)
    {
        Spacing = 10;
        _channelName = channelName;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadCurve();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _saveButton = new Button(Translation.Get("curve.save"));
        _saveButton.Clicked += (_, _) => SaveCurve();
        PackStart(_saveButton, false, false, 0);

        _resetButton = new Button(Translation.Get("curve.reset"));
        _resetButton.Clicked += (_, _) => LoadCurve();
        PackStart(_resetButton, false, false, 0);

        _clearButton = new Button(Translation.Get("curve.clear"));
        _clearButton.Clicked += (_, _) => { _points.Clear(); };
        PackStart(_clearButton, false, false, 0);

        // Hier kannst du später eine echte Kurven-Zeichnung einbauen
    }

    private void LoadCurve()
    {
        var response = _rpc.SendRequest("listChannels");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            foreach (var channel in channels)
            {
                if (channel?["name"]?.ToString() == _channelName && channel["points"] is JsonArray pts)
                {
                    _points.Clear();
                    foreach (var pt in pts)
                    {
                        double x = pt?["x"]?.GetValue<double>() ?? 0.0;
                        double y = pt?["y"]?.GetValue<double>() ?? 0.0;
                        _points.Add((x, y));
                    }
                    break;
                }
            }
        }
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
