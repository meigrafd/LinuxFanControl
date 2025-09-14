using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Views;

public class TriggerEditorAdvanced : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sourceSelector;
    private ComboBoxText _directionSelector;
    private Entry _thresholdEntry;
    private ComboBoxText _targetSelector;
    private Button _createButton;
    private Label _statusLabel;

    public TriggerEditorAdvanced()
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

        _sourceSelector = new ComboBoxText();
        PackStart(_sourceSelector, false, false, 0);

        _directionSelector = new ComboBoxText();
        _directionSelector.AppendText("above");
        _directionSelector.AppendText("below");
        _directionSelector.Active = 0;
        PackStart(_directionSelector, false, false, 0);

        _thresholdEntry = new Entry { PlaceholderText = "Threshold (e.g. 70.0)" };
        PackStart(_thresholdEntry, false, false, 0);

        _targetSelector = new ComboBoxText();
        PackStart(_targetSelector, false, false, 0);

        _createButton = new Button(Translation.Get("trigger.create"));
        _createButton.Clicked += (_, _) => CreateTrigger();
        PackStart(_createButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadOptions()
    {
        _sourceSelector.RemoveAll();
        _targetSelector.RemoveAll();

        var sensors = _rpc.SendRequest("listSensors");
        if (sensors is JsonObject sObj && sObj["result"] is JsonArray sArr)
        {
            foreach (var s in sArr)
            {
                var label = s?["label"]?.ToString() ?? "?";
                var path = s?["path"]?.ToString() ?? "";
                _sourceSelector.AppendText($"{label} ({path})");
            }
        }

        var channels = _rpc.SendRequest("listChannels");
        if (channels is JsonObject cObj && cObj["result"] is JsonArray cArr)
        {
            foreach (var ch in cArr)
            {
                var name = ch?["name"]?.ToString() ?? "?";
                _targetSelector.AppendText(name);
            }
        }

        if (_sourceSelector.Children.Length > 0)
            _sourceSelector.Active = 0;
        if (_targetSelector.Children.Length > 0)
            _targetSelector.Active = 0;
    }

    private void CreateTrigger()
    {
        var source = _sourceSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        var direction = _directionSelector.ActiveText;
        var threshold = _thresholdEntry.Text;
        var target = _targetSelector.ActiveText;

        if (!string.IsNullOrEmpty(source) && !string.IsNullOrEmpty(direction) &&
            double.TryParse(threshold, out var value) && !string.IsNullOrEmpty(target))
        {
            var payload = new JsonObject
            {
                ["source"] = source,
                ["direction"] = direction,
                ["threshold"] = value,
                ["target"] = target
            };
            _rpc.SendRequest("createTrigger", payload);
            _statusLabel.Text = Translation.Get("trigger.success");
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
