using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Views;

public class ChannelDashboard : VBox
{
    private readonly RpcClient _rpc;

    public ChannelDashboard()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadChannels();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        var title = new Label();
        title.SetMarkup($"<b>{Translation.Get("channel.dashboard.title")}</b>");
        PackStart(title, false, false, 0);
    }

    private void LoadChannels()
    {
        var response = _rpc.SendRequest("listChannels");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            foreach (var ch in channels)
            {
                var name = ch?["name"]?.ToString() ?? "Unnamed";
                var mode = ch?["mode"]?.ToString() ?? "?";
                var manual = ch?["manual"]?.ToString() ?? "-";
                var output = ch?["output_label"]?.ToString() ?? "?";

                var line = new Label($"{name} → {output} [{mode}] Manual: {manual}%");
                PackStart(line, false, false, 0);
            }
        }
    }

    private void Redraw()
    {
        BuildUi();
        LoadChannels();
    }
}
