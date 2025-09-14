using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace FanControl.Gui.Tools;

public class SensorDriftAnalyzer : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sensorSelector;
    private Button _analyzeButton;
    private Label _resultLabel;

    public SensorDriftAnalyzer()
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

        _analyzeButton = new Button(Translation.Get("drift.analyze"));
        _analyzeButton.Clicked += (_, _) => AnalyzeDrift();
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

    private void AnalyzeDrift()
    {
        var sensor = _sensorSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        if (!string.IsNullOrEmpty(sensor))
        {
            var payload = new JsonObject { ["path"] = sensor };
            var response = _rpc.SendRequest("analyzeSensorDrift", payload);
            if (response is JsonObject obj && obj["result"] is JsonObject result)
            {
                var drift = result["drift"]?.ToString() ?? "?";
                var trend = result["trend"]?.ToString() ?? "?";
                _resultLabel.Text = Translation.Get("drift.result") + $"\nDrift: {drift} °C\nTrend: {trend}";
            }
            else
            {
                _resultLabel.Text = Translation.Get("drift.error");
            }
        }
        else
        {
            _resultLabel.Text = Translation.Get("drift.invalid");
        }
    }

    private void Redraw()
    {
        _analyzeButton.Label = Translation.Get("drift.analyze");
    }
}
