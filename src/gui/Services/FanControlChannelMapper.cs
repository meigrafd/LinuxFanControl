using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlChannelMapper : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _channelSelector;
    private Entry _deviceEntry;
    private Button _mapButton;
    private Label _statusLabel;

    public FanControlChannelMapper()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadChannels();
    }

    private void BuildUi()
    {
        _channelSelector = new ComboBoxText();
        PackStart(_channelSelector, false, false, 0);

        _deviceEntry = new Entry { PlaceholderText = Translation.Get("channel.device") };
        PackStart(_deviceEntry, false, false, 0);

        _mapButton = new Button(Translation.Get("channel.map"));
        _mapButton.Clicked += (_, _) => MapChannel();
        PackStart(_mapButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadChannels()
    {
        _channelSelector.RemoveAll();

        var response = _rpc.SendRequest("listChannels");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            foreach (var ch in channels)
            {
                var name = ch?["name"]?.ToString() ?? "?";
                _channelSelector.AppendText(name);
            }

            if (_channelSelector.Children.Length > 0)
                _channelSelector.Active = 0;
        }
    }

    private void MapChannel()
    {
        var channel = _channelSelector.ActiveText;
        var device = _deviceEntry.Text;

        if (!string.IsNullOrEmpty(channel) && !string.IsNullOrEmpty(device))
        {
            var payload = new JsonObject
            {
                ["channel"] = channel,
                ["device"] = device
            };
            var response = _rpc.SendRequest("mapFanChannel", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("channel.success")
            : Translation.Get("channel.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("channel.invalid");
        }
    }

    private void Redraw()
    {
        _mapButton.Label = Translation.Get("channel.map");
    }
}
