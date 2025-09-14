using Gtk;
using FanControl.Gui.Services;
using FanControl.Gui.Widgets;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Widgets;

public class ChannelSummaryCard : Frame
{
    private readonly RpcClient _rpc;
    private readonly string _channelName;
    private Label _infoLabel;
    private ChannelCurvePlot _plot;

    public ChannelSummaryCard(string channelName)
    {
        _channelName = channelName;
        _rpc = new RpcClient();

        Label = channelName;
        BorderWidth = 6;

        var vbox = new VBox { Spacing = 6 };

        _infoLabel = new Label();
        vbox.PackStart(_infoLabel, false, false, 0);

        _plot = new ChannelCurvePlot(channelName);
        vbox.PackStart(_plot, true, true, 0);

        Add(vbox);

        Translation.LanguageChanged += Redraw;
        LoadChannelInfo();
    }

    private void LoadChannelInfo()
    {
        var response = _rpc.SendRequest("listChannels");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            foreach (var ch in channels)
            {
                if (ch?["name"]?.ToString() == _channelName)
                {
                    var mode = ch["mode"]?.ToString() ?? "?";
                    var output = ch["output_label"]?.ToString() ?? "?";
                    var sensor = ch["sensor_path"]?.ToString() ?? "?";
                    var manual = ch["manual"]?.ToString() ?? "-";

                    _infoLabel.Text = $"{mode} → {output}, Sensor: {sensor}, Manual: {manual}%";
                    break;
                }
            }
        }
    }

    private void Redraw()
    {
        LoadChannelInfo();
    }
}
