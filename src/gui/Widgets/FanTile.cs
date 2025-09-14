using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Widgets;

public class FanTile : Frame
{
    private readonly string _label;
    private readonly RpcClient _rpc;
    private Label _rpmLabel;
    private Scale _manualSlider;
    private Button _applyButton;

    public FanTile(string label)
    {
        _label = label;
        _rpc = new RpcClient();

        Label = label;
        BorderWidth = 6;

        var vbox = new VBox { Spacing = 4 };

        _rpmLabel = new Label("RPM: --");
        vbox.PackStart(_rpmLabel, false, false, 0);

        _manualSlider = new Scale(Orientation.Horizontal)
        {
            DrawValue = true,
            Digits = 0,
            ValuePos = PositionType.Right,
            Adjustment = new Adjustment(50, 0, 100, 1, 10, 0)
        };
        vbox.PackStart(_manualSlider, false, false, 0);

        _applyButton = new Button(Translation.Get("fan.apply"));
        _applyButton.Clicked += (_, _) => ApplyManualSpeed();
        vbox.PackStart(_applyButton, false, false, 0);

        Add(vbox);

        Translation.LanguageChanged += Redraw;
        UpdateRpm();
    }

    private void UpdateRpm()
    {
        try
        {
            var response = _rpc.SendRequest("getFanSpeeds");
            if (response is JsonObject obj && obj["result"] is JsonArray fans)
            {
                foreach (var fan in fans)
                {
                    if (fan?["label"]?.ToString() == _label)
                    {
                        var rpm = fan["rpm"]?.ToString() ?? "--";
                        _rpmLabel.Text = $"RPM: {rpm}";
                        break;
                    }
                }
            }
        }
        catch
        {
            _rpmLabel.Text = "RPM: ?";
        }
    }

    private void ApplyManualSpeed()
    {
        var value = (int)_manualSlider.Value;
        var payload = new JsonObject
        {
            ["name"] = _label,
            ["value"] = value
        };
        _rpc.SendRequest("setChannelManual", payload);
    }

    private void Redraw()
    {
        _applyButton.Label = Translation.Get("fan.apply");
    }
}
