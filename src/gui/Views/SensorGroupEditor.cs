using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Views;

public class SensorGroupEditor : VBox
{
    private readonly RpcClient _rpc;

    private Entry _groupNameEntry;
    private ComboBoxText _sensorSelector;
    private Button _addButton;
    private Label _statusLabel;

    public SensorGroupEditor()
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

        _groupNameEntry = new Entry { PlaceholderText = Translation.Get("sensor.group.name") };
        PackStart(_groupNameEntry, false, false, 0);

        _sensorSelector = new ComboBoxText();
        PackStart(_sensorSelector, false, false, 0);

        _addButton = new Button(Translation.Get("sensor.group.add"));
        _addButton.Clicked += (_, _) => AddToGroup();
        PackStart(_addButton, false, false, 0);

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

    private void AddToGroup()
    {
        var group = _groupNameEntry.Text;
        var sensor = _sensorSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');

        if (!string.IsNullOrEmpty(group) && !string.IsNullOrEmpty(sensor))
        {
            var payload = new JsonObject
            {
                ["group"] = group,
                ["path"] = sensor
            };
            _rpc.SendRequest("addSensorToGroup", payload);
            _statusLabel.Text = Translation.Get("sensor.group.success");
        }
        else
        {
            _statusLabel.Text = Translation.Get("sensor.group.invalid");
        }
    }

    private void Redraw()
    {
        _addButton.Label = Translation.Get("sensor.group.add");
    }
}
