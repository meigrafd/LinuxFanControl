using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class SensorAnomalyScanner : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sensorSelector;
    private Button _scanButton;
    private TextView _resultView;

    public SensorAnomalyScanner()
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

        _scanButton = new Button(Translation.Get("anomaly.scan"));
        _scanButton.Clicked += (_, _) => ScanSensor();
        PackStart(_scanButton, false, false, 0);

        _resultView = new TextView
        {
            Editable = false,
            WrapMode = WrapMode.Word
        };
        var scroll = new ScrolledWindow();
        scroll.Add(_resultView);
        scroll.SetSizeRequest(500, 200);
        PackStart(scroll, true, true, 0);
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
            var response = _rpc.SendRequest("scanSensorAnomalies", payload);
            if (response is JsonObject obj && obj["result"] is JsonArray anomalies)
            {
                if (anomalies.Count == 0)
                {
                    _resultView.Buffer.Text = Translation.Get("anomaly.clean");
                }
                else
                {
                    var lines = string.Join("\n", anomalies.Select(a => "• " + a?.ToString()));
                    _resultView.Buffer.Text = Translation.Get("anomaly.found") + "\n" + lines;
                }
            }
            else
            {
                _resultView.Buffer.Text = Translation.Get("anomaly.error");
            }
        }
        else
        {
            _resultView.Buffer.Text = Translation.Get("anomaly.invalid");
        }
    }

    private void Redraw()
    {
        _scanButton.Label = Translation.Get("anomaly.scan");
    }
}
