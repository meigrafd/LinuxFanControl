using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Diagnostics;

namespace FanControl.Gui.Tools;

public class SensorLatencyTester : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sensorSelector;
    private Button _testButton;
    private Label _resultLabel;

    public SensorLatencyTester()
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

        _testButton = new Button(Translation.Get("latency.test"));
        _testButton.Clicked += (_, _) => RunLatencyTest();
        PackStart(_testButton, false, false, 0);

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

    private void RunLatencyTest()
    {
        var sensor = _sensorSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        if (!string.IsNullOrEmpty(sensor))
        {
            var payload = new JsonObject { ["path"] = sensor };
            var stopwatch = Stopwatch.StartNew();
            var response = _rpc.SendRequest("readSensorValue", payload);
            stopwatch.Stop();

            if (response is JsonObject obj && obj["result"] != null)
            {
                var value = obj["result"]?.ToString() ?? "?";
                var ms = stopwatch.ElapsedMilliseconds;
                _resultLabel.Text = Translation.Get("latency.result") + $"\n→ {value} in {ms} ms";
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
        _testButton.Label = Translation.Get("latency.test");
    }
}
