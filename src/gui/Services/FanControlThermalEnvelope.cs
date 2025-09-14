using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlThermalEnvelope : VBox
{
    private readonly RpcClient _rpc;

    private Entry _minEntry;
    private Entry _maxEntry;
    private Button _applyButton;
    private Label _statusLabel;

    public FanControlThermalEnvelope()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        _minEntry = new Entry { PlaceholderText = Translation.Get("envelope.min") };
        PackStart(_minEntry, false, false, 0);

        _maxEntry = new Entry { PlaceholderText = Translation.Get("envelope.max") };
        PackStart(_maxEntry, false, false, 0);

        _applyButton = new Button(Translation.Get("envelope.apply"));
        _applyButton.Clicked += (_, _) => ApplyEnvelope();
        PackStart(_applyButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void ApplyEnvelope()
    {
        var minText = _minEntry.Text;
        var maxText = _maxEntry.Text;

        if (double.TryParse(minText, out var min) && double.TryParse(maxText, out var max) && min < max)
        {
            var payload = new JsonObject
            {
                ["min"] = min,
                ["max"] = max
            };
            var response = _rpc.SendRequest("setThermalEnvelope", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("envelope.success")
            : Translation.Get("envelope.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("envelope.invalid");
        }
    }

    private void Redraw()
    {
        _applyButton.Label = Translation.Get("envelope.apply");
    }
}
