using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace FanControl.Gui.Tools;

public class SensorAnomalyDetector : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sensorSelector;
    private Button _scanButton;
    private Label _resultLabel;

    public SensorAnomalyDetector()
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

        _scanButton = new Button(Translation.Get("anomaly.scan"));
        _scanButton.Clicked += (_, _) => ScanSensor();
        PackStart(_scanButton, false, false, 0);

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

    private void ScanSensor()
    {
        var sensor = _sensorSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        if (!string.IsNullOrEmpty(sensor))
        {
            var payload = new JsonObject { ["path"] = sensor };
            var response = _rpc.SendRequest("detectSensorAnomalies", payload);
            if (response is JsonObject obj && obj["result"] is JsonArray anomalies)
            {
                if (anomalies.Count == 0)
                    _resultLabel.Text = Translation.Get("anomaly.none");
                else
                    _resultLabel.Text = Translation.Get("anomaly.found") + $"\n→ {anomalies.Count} anomalies";
            }
            else
            {
                _resultLabel.Text = Translation.Get("anomaly.error");
            }
        }
    }

    private void Redraw()
    {
        _scanButton.Label = Translation.Get("anomaly.scan");
    }
}
