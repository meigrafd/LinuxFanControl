using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;
using FanControl.Gui.Widgets;

namespace FanControl.Gui.Views;

public class SensorGroupView : VBox
{
    private readonly RpcClient _rpc;

    public SensorGroupView()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadSensors();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        var title = new Label();
        title.SetMarkup($"<b>{Translation.Get("sensor.group.title")}</b>");
        PackStart(title, false, false, 0);
    }

    private void LoadSensors()
    {
        var response = _rpc.SendRequest("listSensors");
        if (response is JsonObject obj && obj["result"] is JsonArray sensors)
        {
            foreach (var sensor in sensors)
            {
                var path = sensor?["path"]?.ToString();
                if (!string.IsNullOrEmpty(path))
                {
                    var tile = new SensorTile(path);
                    PackStart(tile, false, false, 0);
                }
            }
        }
    }

    private void Redraw()
    {
        BuildUi();
        LoadSensors();
    }
}
