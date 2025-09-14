using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class SensorLatencyProfiler : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sensorSelector;
    private Button _profileButton;
    private Label _resultLabel;

    public SensorLatencyProfiler()
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

        _profileButton = new Button(Translation.Get("latency.run"));
        _profileButton.Clicked += (_, _) => ProfileLatency();
        PackStart(_profileButton, false, false, 0);

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

    private void ProfileLatency()
    {
        var sensor = _sensorSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        if (!string.IsNullOrEmpty(sensor))
        {
            var payload = new JsonObject { ["path"] = sensor };
            var response = _rpc.SendRequest("profileSensorLatency", payload);
            if (response is JsonObject obj && obj["result"] is JsonObject result)
            {
                var avg = result["avg_ms"]?.ToString() ?? "?";
                var max = result["max_ms"]?.ToString() ?? "?";
                var min = result["min_ms"]?.ToString() ?? "?";
                _resultLabel.Text = Translation.Get("latency.result") + $"\nMin: {min} ms\nAvg: {avg} ms\nMax: {max} ms";
            }
            else
            {
                _resultLabel.Text = Translation.Get("latency.error");
            }
        }
        else
        {
            _resultLabel.Text = Translation.Get("latency.invalid");
        }
    }

    private void Redraw()
    {
        _profileButton.Label = Translation.Get("latency.run");
    }
}
