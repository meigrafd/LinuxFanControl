using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class SensorCalibrationWizard : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sensorSelector;
    private Entry _offsetEntry;
    private Button _applyButton;
    private Label _statusLabel;

    public SensorCalibrationWizard()
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

        _offsetEntry = new Entry { PlaceholderText = Translation.Get("calibration.offset") };
        PackStart(_offsetEntry, false, false, 0);

        _applyButton = new Button(Translation.Get("calibration.apply"));
        _applyButton.Clicked += (_, _) => ApplyCalibration();
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

    private void ApplyCalibration()
    {
        var sensor = _sensorSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        var offsetText = _offsetEntry.Text;

        if (!string.IsNullOrEmpty(sensor) && double.TryParse(offsetText, out var offset))
        {
            var payload = new JsonObject
            {
                ["path"] = sensor,
                ["offset"] = offset
            };
            var response = _rpc.SendRequest("calibrateSensor", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("calibration.success")
            : Translation.Get("calibration.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("calibration.invalid");
        }
    }

    private void Redraw()
    {
        _applyButton.Label = Translation.Get("calibration.apply");
    }
}
