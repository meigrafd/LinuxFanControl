using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlOverridePanel : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _channelSelector;
    private Scale _pwmSlider;
    private Button _applyButton;
    private Button _resetButton;
    private Label _statusLabel;

    public FanControlOverridePanel()
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

        _pwmSlider = new Scale(Orientation.Horizontal)
        {
            MinValue = 0,
            MaxValue = 100,
            Value = 50,
            Digits = 0,
            DrawValue = true
        };
        PackStart(_pwmSlider, false, false, 0);

        _applyButton = new Button(Translation.Get("override.apply"));
        _applyButton.Clicked += (_, _) => ApplyOverride();
        PackStart(_applyButton, false, false, 0);

        _resetButton = new Button(Translation.Get("override.reset"));
        _resetButton.Clicked += (_, _) => ResetOverride();
        PackStart(_resetButton, false, false, 0);

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
                var name = ch?["name"]?.ToString() ?? "";
                _channelSelector.AppendText(name);
            }

            if (_channelSelector.Children.Length > 0)
                _channelSelector.Active = 0;
        }
    }

    private void ApplyOverride()
    {
        var channel = _channelSelector.ActiveText;
        var pwm = (int)_pwmSlider.Value;

        if (!string.IsNullOrEmpty(channel))
        {
            var payload = new JsonObject
            {
                ["name"] = channel,
                ["pwm"] = pwm
            };
            var response = _rpc.SendRequest("overrideChannelPwm", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("override.success")
            : Translation.Get("override.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("override.invalid");
        }
    }

    private void ResetOverride()
    {
        var channel = _channelSelector.ActiveText;
        if (!string.IsNullOrEmpty(channel))
        {
            var payload = new JsonObject { ["name"] = channel };
            var response = _rpc.SendRequest("clearChannelOverride", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("override.cleared")
            : Translation.Get("override.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("override.invalid");
        }
    }

    private void Redraw()
    {
        _applyButton.Label = Translation.Get("override.apply");
        _resetButton.Label = Translation.Get("override.reset");
    }
}
