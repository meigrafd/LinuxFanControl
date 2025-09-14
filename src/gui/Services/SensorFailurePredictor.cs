using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class SensorFailurePredictor : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sensorSelector;
    private Button _analyzeButton;
    private Label _resultLabel;

    public SensorFailurePredictor()
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

        _analyzeButton = new Button(Translation.Get("failure.analyze"));
        _analyzeButton.Clicked += (_, _) => AnalyzeFailure();
        PackStart(_analyzeButton, false, false, 0);

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

    private void AnalyzeFailure()
    {
        var sensor = _sensorSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        if (!string.IsNullOrEmpty(sensor))
        {
            var payload = new JsonObject { ["path"] = sensor };
            var response = _rpc.SendRequest("predictSensorFailure", payload);
            if (response is JsonObject obj && obj["result"] is JsonObject result)
            {
                var risk = result["risk"]?.ToString() ?? "?";
                var reason = result["reason"]?.ToString() ?? "";
                _resultLabel.Text = Translation.Get("failure.result") + $"\nRisk: {risk}\nReason: {reason}";
            }
            else
            {
                _resultLabel.Text = Translation.Get("failure.error");
            }
        }
        else
        {
            _resultLabel.Text = Translation.Get("failure.invalid");
        }
    }

    private void Redraw()
    {
        _analyzeButton.Label = Translation.Get("failure.analyze");
    }
}
