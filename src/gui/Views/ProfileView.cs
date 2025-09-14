using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Views;

public class ProfileView : VBox
{
    private Label _titleLabel;
    private RpcClient _rpc;

    public ProfileView()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadProfiles();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _titleLabel = new Label();
        _titleLabel.SetMarkup($"<b>{Translation.Get("sidebar.curve")}</b>");
        PackStart(_titleLabel, false, false, 0);
    }

    private void LoadProfiles()
    {
        var response = _rpc.SendRequest("listChannels");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            foreach (var channel in channels)
            {
                string name = channel?["name"]?.ToString() ?? "Unnamed";
                string mode = channel?["mode"]?.ToString() ?? "?";
                string sensor = channel?["sensor_path"]?.ToString() ?? "?";
                string output = channel?["output_label"]?.ToString() ?? "?";

                var label = new Label($"{name} → {output} ({mode}, Sensor: {sensor})");
                PackStart(label, false, false, 0);
            }
        }
    }

    private void Redraw()
    {
        BuildUi();
        LoadProfiles();
    }
}
