using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlTriggerDesigner : VBox
{
    private readonly RpcClient _rpc;

    private Entry _triggerNameEntry;
    private ComboBoxText _sensorSelector;
    private Entry _thresholdEntry;
    private ComboBoxText _actionSelector;
    private Button _createButton;
    private Label _statusLabel;

    public FanControlTriggerDesigner()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadSensors();
    }

    private void BuildUi()
    {
        _triggerNameEntry = new Entry { PlaceholderText = Translation.Get("trigger.name") };
        PackStart(_triggerNameEntry, false, false, 0);

        _sensorSelector = new ComboBoxText();
        PackStart(_sensorSelector, false, false, 0);

        _thresholdEntry = new Entry { PlaceholderText = Translation.Get("trigger.threshold") };
        PackStart(_thresholdEntry, false, false, 0);

        _actionSelector = new ComboBoxText();
        _actionSelector.AppendText("activateProfile");
        _actionSelector.AppendText("sendAlert");
        _actionSelector.AppendText("logEvent");
        _actionSelector.Active = 0;
        PackStart(_actionSelector, false, false, 0);

        _createButton = new Button(Translation.Get("trigger.create"));
        _createButton.Clicked += (_, _) => CreateTrigger();
        PackStart(_createButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
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

    private void CreateTrigger()
    {
        var name = _triggerNameEntry.Text;
        var sensor = _sensorSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        var thresholdText = _thresholdEntry.Text;
        var action = _actionSelector.ActiveText;

        if (!string.IsNullOrEmpty(name) && !string.IsNullOrEmpty(sensor) &&
            double.TryParse(thresholdText, out var threshold) && !string.IsNullOrEmpty(action))
        {
            var payload = new JsonObject
            {
                ["name"] = name,
                ["sensor"] = sensor,
                ["threshold"] = threshold,
                ["action"] = action
            };
            var response = _rpc.SendRequest("createTrigger", payload);
            if (response is JsonObject obj && obj["result"]?.ToString() == "ok")
            {
                _statusLabel.Text = Translation.Get("trigger.success");
            }
            else
            {
                _statusLabel.Text = Translation.Get("trigger.error");
            }
        }
        else
        {
            _statusLabel.Text = Translation.Get("trigger.invalid");
        }
    }

    private void Redraw()
    {
        _createButton.Label = Translation.Get("trigger.create");
    }
}
