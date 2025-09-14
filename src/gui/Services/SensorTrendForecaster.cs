using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class SensorTrendForecaster : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sensorSelector;
    private Entry _horizonEntry;
    private Button _forecastButton;
    private TextView _resultView;

    public SensorTrendForecaster()
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

        _horizonEntry = new Entry { PlaceholderText = Translation.Get("forecast.horizon") };
        PackStart(_horizonEntry, false, false, 0);

        _forecastButton = new Button(Translation.Get("forecast.run"));
        _forecastButton.Clicked += (_, _) => RunForecast();
        PackStart(_forecastButton, false, false, 0);

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

    private void RunForecast()
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
            var response = _rpc.SendRequest("forecastSensorTrend", payload);
            if (response is JsonObject obj && obj["result"] is JsonArray forecast)
            {
                var lines = string.Join("\n", forecast.Select(f => "• " + f?.ToString()));
                _resultView.Buffer.Text = Translation.Get("forecast.result") + "\n" + lines;
            }
            else
            {
                _resultView.Buffer.Text = Translation.Get("forecast.error");
            }
        }
        else
        {
            _resultView.Buffer.Text = Translation.Get("forecast.invalid");
        }
    }

    private void Redraw()
    {
        _forecastButton.Label = Translation.Get("forecast.run");
    }
}
