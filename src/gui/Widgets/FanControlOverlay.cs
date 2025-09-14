using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Widgets;

public class FanControlOverlay : Window
{
    private readonly RpcClient _rpc;
    private ComboBoxText _channelSelector;
    private Scale _speedSlider;
    private Button _applyButton;

    public FanControlOverlay() : base(WindowType.Popup)
    {
        Title = Translation.Get("overlay.title");
        SetDefaultSize(300, 120);
        SetPosition(WindowPosition.CenterAlways);

        _rpc = new RpcClient();
        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadChannels();
        ShowAll();
    }

    private void BuildUi()
    {
        var vbox = new VBox { Spacing = 8, BorderWidth = 10 };

        _channelSelector = new ComboBoxText();
        vbox.PackStart(_channelSelector, false, false, 0);

        _speedSlider = new Scale(Orientation.Horizontal)
        {
            DrawValue = true,
            Digits = 0,
            ValuePos = PositionType.Right,
            Adjustment = new Adjustment(50, 0, 100, 1, 10, 0)
        };
        vbox.PackStart(_speedSlider, false, false, 0);

        _applyButton = new Button(Translation.Get("overlay.apply"));
        _applyButton.Clicked += (_, _) => ApplySpeed();
        vbox.PackStart(_applyButton, false, false, 0);

        Add(vbox);
    }

    private void LoadChannels()
    {
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

    private void ApplySpeed()
    {
        var selected = _channelSelector.ActiveText;
        if (!string.IsNullOrEmpty(selected))
        {
            var payload = new JsonObject
            {
                ["name"] = selected,
                ["value"] = (int)_speedSlider.Value
            };
            _rpc.SendRequest("setChannelManual", payload);
        }
    }

    private void Redraw()
    {
        _applyButton.Label = Translation.Get("overlay.apply");
    }
}
