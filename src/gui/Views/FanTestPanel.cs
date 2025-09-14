using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Views;

public class FanTestPanel : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _fanSelector;
    private Scale _speedSlider;
    private Button _applyButton;

    public FanTestPanel()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadFans();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _fanSelector = new ComboBoxText();
        PackStart(_fanSelector, false, false, 0);

        _speedSlider = new Scale(Orientation.Horizontal)
        {
            DrawValue = true,
            Digits = 0,
            ValuePos = PositionType.Right,
            Adjustment = new Adjustment(50, 0, 100, 1, 10, 0)
        };
        PackStart(_speedSlider, false, false, 0);

        _applyButton = new Button(Translation.Get("fan.test.apply"));
        _applyButton.Clicked += (_, _) => ApplySpeed();
        PackStart(_applyButton, false, false, 0);
    }

    private void LoadFans()
    {
        _fanSelector.RemoveAll();

        var response = _rpc.SendRequest("listPwms");
        if (response is JsonObject obj && obj["result"] is JsonArray fans)
        {
            foreach (var fan in fans)
            {
                var label = fan?["label"]?.ToString() ?? "?";
                _fanSelector.AppendText(label);
            }

            if (_fanSelector.Children.Length > 0)
                _fanSelector.Active = 0;
        }
    }

    private void ApplySpeed()
    {
        var selected = _fanSelector.ActiveText;
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
        BuildUi();
        LoadFans();
    }
}
