using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Views;

public class SensorCalibrationWizard : VBox
{
    private readonly RpcClient _rpc;
    private ComboBoxText _sensorSelector;
    private Button _calibrateButton;
    private Label _resultLabel;

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
        foreach (var child in Children)
            Remove(child);

        _sensorSelector = new ComboBoxText();
        PackStart(_sensorSelector, false, false, 0);

        _calibrateButton = new Button(Translation.Get("calibration.run"));
        _calibrateButton.Clicked += (_, _) => RunCalibration();
        PackStart(_calibrateButton, false, false, 0);

        _resultLabel = new Label();
        PackStart(_resultLabel, false, false, 0);
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

    private void RunCalibration()
    {
        var selected = _sensorSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        if (!string.IsNullOrEmpty(selected))
        {
            var payload = new JsonObject { ["path"] = selected };
            var response = _rpc.SendRequest("calibrateSensor", payload);
            if (response is JsonObject obj && obj["result"] is JsonObject result)
            {
                var ok = result["ok"]?.ToString() ?? "false";
                var offset = result["offset"]?.ToString() ?? "?";
                _resultLabel.Text = $"OK: {ok}, Offset: {offset}";
            }
            else
            {
                _resultLabel.Text = Translation.Get("calibration.error");
            }
        }
    }

    private void Redraw()
    {
        _calibrateButton.Label = Translation.Get("calibration.run");
    }
}
