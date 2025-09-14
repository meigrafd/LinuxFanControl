using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Widgets;

public class ChannelList : VBox
{
    private readonly RpcClient _rpc;

    public ChannelList()
    {
        Spacing = 8;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        LoadChannels();
    }

    private void LoadChannels()
    {
        foreach (var child in Children)
            Remove(child);

        var response = _rpc.SendRequest("listChannels");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            foreach (var ch in channels)
            {
                var name = ch?["name"]?.ToString() ?? "Unnamed";
                var mode = ch?["mode"]?.ToString() ?? "?";
                var sensor = ch?["sensor_path"]?.ToString() ?? "?";
                var output = ch?["output_label"]?.ToString() ?? "?";

                var label = new Label($"{name} → {output} ({mode}, Sensor: {sensor})");
                PackStart(label, false, false, 0);
            }
        }
    }

    private void Redraw()
    {
        LoadChannels();
    }
}
