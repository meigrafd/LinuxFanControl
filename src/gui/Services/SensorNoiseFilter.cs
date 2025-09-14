using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class SensorNoiseFilter : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sensorSelector;
    private ComboBoxText _filterTypeSelector;
    private Entry _parameterEntry;
    private Button _applyButton;
    private Label _statusLabel;

    public SensorNoiseFilter()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadSensors();
    }

    private void BuildUi()
    {
        _sensorSelector = new ComboBoxText();
        PackStart(_sensorSelector, false, false, 0);

        _filterTypeSelector = new ComboBoxText();
        _filterTypeSelector.AppendText("moving-average");
        _filterTypeSelector.AppendText("exponential");
        _filterTypeSelector.AppendText("median");
        _filterTypeSelector.Active = 0;
        PackStart(_filterTypeSelector, false, false, 0);

        _parameterEntry = new Entry { PlaceholderText = Translation.Get("filter.parameter") };
        PackStart(_parameterEntry, false, false, 0);

        _applyButton = new Button(Translation.Get("filter.apply"));
        _applyButton.Clicked += (_, _) => ApplyFilter();
        PackStart(_applyButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadSensors()
    {
        _sensorSelector.RemoveAll();

        var response = _rpc.SendRequest("listSensors");
        if (response is JsonObject obj && obj["result"] is JsonArray sensors)
        {
            foreach (var s in sensors)
            {
                var label = s?["label"]?.ToString() ?? "?";
                var path = s?["path"]?.ToString() ?? "";
                _sensorSelector.AppendText($"{label} ({path})");
            }

            if (_sensorSelector.Children.Length > 0)
                _sensorSelector.Active = 0;
        }
    }

    private void ApplyFilter()
    {
        var sensor = _sensorSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        var type = _filterTypeSelector.ActiveText;
        var paramText = _parameterEntry.Text;

        if (!string.IsNullOrEmpty(sensor) && !string.IsNullOrEmpty(type) && double.TryParse(paramText, out var param))
        {
            var payload = new JsonObject
            {
                ["path"] = sensor,
                ["type"] = type,
                ["parameter"] = param
            };
            var response = _rpc.SendRequest("setSensorFilter", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("filter.success")
            : Translation.Get("filter.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("filter.invalid");
        }
    }

    private void Redraw()
    {
        _applyButton.Label = Translation.Get("filter.apply");
    }
}
