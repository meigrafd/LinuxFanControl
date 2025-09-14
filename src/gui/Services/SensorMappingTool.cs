using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class SensorMappingTool : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sensorSelector;
    private ComboBoxText _pwmSelector;
    private Button _mapButton;
    private Label _statusLabel;

    public SensorMappingTool()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadOptions();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _sensorSelector = new ComboBoxText();
        PackStart(_sensorSelector, false, false, 0);

        _pwmSelector = new ComboBoxText();
        PackStart(_pwmSelector, false, false, 0);

        _mapButton = new Button(Translation.Get("mapping.assign"));
        _mapButton.Clicked += (_, _) => AssignMapping();
        PackStart(_mapButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadOptions()
    {
        _sensorSelector.RemoveAll();
        _pwmSelector.RemoveAll();

        var sensors = _rpc.SendRequest("listSensors");
        if (sensors is JsonObject sObj && sObj["result"] is JsonArray sArr)
        {
            foreach (var s in sArr)
            {
                var label = s?["label"]?.ToString() ?? "?";
                var path = s?["path"]?.ToString() ?? "";
                _sensorSelector.AppendText($"{label} ({path})");
            }
        }

        var pwms = _rpc.SendRequest("listPwms");
        if (pwms is JsonObject pObj && pObj["result"] is JsonArray pArr)
        {
            foreach (var p in pArr)
            {
                var label = p?["label"]?.ToString() ?? "?";
                _pwmSelector.AppendText(label);
            }
        }

        if (_sensorSelector.Children.Length > 0)
            _sensorSelector.Active = 0;
        if (_pwmSelector.Children.Length > 0)
            _pwmSelector.Active = 0;
    }

    private void AssignMapping()
    {
        var sensor = _sensorSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        var pwm = _pwmSelector.ActiveText;

        if (!string.IsNullOrEmpty(sensor) && !string.IsNullOrEmpty(pwm))
        {
            var payload = new JsonObject
            {
                ["sensor_path"] = sensor,
                ["pwm_label"] = pwm
            };
            _rpc.SendRequest("mapSensorToPwm", payload);
            _statusLabel.Text = Translation.Get("mapping.success");
        }
        else
        {
            _statusLabel.Text = Translation.Get("mapping.invalid");
        }
    }

    private void Redraw()
    {
        _mapButton.Label = Translation.Get("mapping.assign");
    }
}
