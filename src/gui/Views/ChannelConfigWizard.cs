using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Views;

public class ChannelConfigWizard : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sensorSelector;
    private ComboBoxText _outputSelector;
    private Entry _nameEntry;
    private Button _createButton;

    public ChannelConfigWizard()
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

        _nameEntry = new Entry { PlaceholderText = Translation.Get("channel.name") };
        PackStart(_nameEntry, false, false, 0);

        _sensorSelector = new ComboBoxText();
        PackStart(_sensorSelector, false, false, 0);

        _outputSelector = new ComboBoxText();
        PackStart(_outputSelector, false, false, 0);

        _createButton = new Button(Translation.Get("channel.create"));
        _createButton.Clicked += (_, _) => CreateChannel();
        PackStart(_createButton, false, false, 0);
    }

    private void LoadOptions()
    {
        _sensorSelector.RemoveAll();
        _outputSelector.RemoveAll();

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

        var outputs = _rpc.SendRequest("listPwms");
        if (outputs is JsonObject oObj && oObj["result"] is JsonArray oArr)
        {
            foreach (var o in oArr)
            {
                var label = o?["label"]?.ToString() ?? "?";
                _outputSelector.AppendText(label);
            }
        }

        if (_sensorSelector.Children.Length > 0)
            _sensorSelector.Active = 0;
        if (_outputSelector.Children.Length > 0)
            _outputSelector.Active = 0;
    }

    private void CreateChannel()
    {
        var name = _nameEntry.Text;
        var sensor = _sensorSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        var output = _outputSelector.ActiveText;

        if (!string.IsNullOrEmpty(name) && !string.IsNullOrEmpty(sensor) && !string.IsNullOrEmpty(output))
        {
            var payload = new JsonObject
            {
                ["name"] = name,
                ["sensor_path"] = sensor,
                ["output_label"] = output
            };
            _rpc.SendRequest("createChannel", payload);
        }
    }

    private void Redraw()
    {
        BuildUi();
        LoadOptions();
    }
}
