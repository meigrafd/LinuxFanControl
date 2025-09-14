using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class SensorNoiseFilterEditor : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sensorSelector;
    private Entry _windowEntry;
    private Entry _thresholdEntry;
    private Button _applyButton;
    private Label _statusLabel;

    public SensorNoiseFilterEditor()
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

        _windowEntry = new Entry { PlaceholderText = Translation.Get("filter.window") };
        PackStart(_windowEntry, false, false, 0);

        _thresholdEntry = new Entry { PlaceholderText = Translation.Get("filter.threshold") };
        PackStart(_thresholdEntry, false, false, 0);

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
        var windowText = _windowEntry.Text;
        var thresholdText = _thresholdEntry.Text;

        if (!string.IsNullOrEmpty(sensor) &&
            int.TryParse(windowText, out var window) &&
            double.TryParse(thresholdText, out var threshold))
        {
            var payload = new JsonObject
            {
                ["path"] = sensor,
                ["window"] = window,
                ["threshold"] = threshold
            };
            var response = _rpc.SendRequest("setSensorNoiseFilter", payload);
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
