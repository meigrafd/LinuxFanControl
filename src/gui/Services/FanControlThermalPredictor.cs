using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlThermalPredictor : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sensorSelector;
    private Entry _horizonEntry;
    private Button _predictButton;
    private Label _resultLabel;

    public FanControlThermalPredictor()
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

        _horizonEntry = new Entry { PlaceholderText = Translation.Get("predict.horizon") };
        PackStart(_horizonEntry, false, false, 0);

        _predictButton = new Button(Translation.Get("predict.run"));
        _predictButton.Clicked += (_, _) => RunPrediction();
        PackStart(_predictButton, false, false, 0);

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

    private void RunPrediction()
    {
        var sensor = _sensorSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        var horizonText = _horizonEntry.Text;

        if (!string.IsNullOrEmpty(sensor) && int.TryParse(horizonText, out var horizon))
        {
            var payload = new JsonObject
            {
                ["path"] = sensor,
                ["minutes"] = horizon
            };
            var response = _rpc.SendRequest("predictTemperatureTrend", payload);
            if (response is JsonObject obj && obj["result"] is JsonObject result)
            {
                var forecast = result["forecast"]?.ToString() ?? "?";
                var confidence = result["confidence"]?.ToString() ?? "?";
                _resultLabel.Text = Translation.Get("predict.result") + $"\nForecast: {forecast} °C\nConfidence: {confidence}%";
            }
            else
            {
                _resultLabel.Text = Translation.Get("predict.error");
            }
        }
        else
        {
            _resultLabel.Text = Translation.Get("predict.invalid");
        }
    }

    private void Redraw()
    {
        _predictButton.Label = Translation.Get("predict.run");
    }
}
