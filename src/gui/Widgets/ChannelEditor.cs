using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Widgets;

public class ChannelEditor : VBox
{
    private readonly RpcClient _rpc;
    private readonly string _channelName;

    private ComboBoxText _modeSelector;
    private Scale _manualSlider;
    private Button _applyButton;

    public ChannelEditor(string channelName)
    {
        Spacing = 10;
        _channelName = channelName;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadChannel();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _modeSelector = new ComboBoxText();
        _modeSelector.AppendText("Auto");
        _modeSelector.AppendText("Manual");
        _modeSelector.Active = 0;
        PackStart(_modeSelector, false, false, 0);

        _manualSlider = new Scale(Orientation.Horizontal)
        {
            DrawValue = true,
            Digits = 0,
            ValuePos = PositionType.Right,
            Adjustment = new Adjustment(50, 0, 100, 1, 10, 0)
        };
        PackStart(_manualSlider, false, false, 0);

        _applyButton = new Button(Translation.Get("channel.apply"));
        _applyButton.Clicked += (_, _) => ApplyChanges();
        PackStart(_applyButton, false, false, 0);
    }

    private void LoadChannel()
    {
        var response = _rpc.SendRequest("listChannels");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            foreach (var ch in channels)
            {
                if (ch?["name"]?.ToString() == _channelName)
                {
                    var mode = ch["mode"]?.ToString() ?? "Auto";
                    var manual = ch["manual"]?.GetValue<double>() ?? 50.0;

                    _modeSelector.Active = mode == "Manual" ? 1 : 0;
                    _manualSlider.Value = manual;
                    break;
                }
            }
        }
    }

    private void ApplyChanges()
    {
        string mode = _modeSelector.ActiveText ?? "Auto";

        if (mode == "Manual")
        {
            var payload = new JsonObject
            {
                ["name"] = _channelName,
                ["value"] = (int)_manualSlider.Value
            };
            _rpc.SendRequest("setChannelManual", payload);
        }
        else
        {
            var payload = new JsonObject
            {
                ["name"] = _channelName,
                ["mode"] = "Auto"
            };
            _rpc.SendRequest("setChannelMode", payload);
        }
    }

    private void Redraw()
    {
        _applyButton.Label = Translation.Get("channel.apply");
    }
}
