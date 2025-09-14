using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Widgets;

public class TriggerEditor : VBox
{
    private RpcClient _rpc;
    private string _channelName;

    private ComboBoxText _directionSelector;
    private Entry _thresholdEntry;
    private Entry _targetProfileEntry;
    private Button _applyButton;

    public TriggerEditor(string channelName)
    {
        Spacing = 10;
        _channelName = channelName;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _directionSelector = new ComboBoxText();
        _directionSelector.AppendText(Translation.Get("trigger.direction.greater"));
        _directionSelector.AppendText(Translation.Get("trigger.direction.less"));
        _directionSelector.Active = 0;
        PackStart(_directionSelector, false, false, 0);

        _thresholdEntry = new Entry { PlaceholderText = "Threshold value" };
        PackStart(_thresholdEntry, false, false, 0);

        _targetProfileEntry = new Entry { PlaceholderText = Translation.Get("trigger.target") };
        PackStart(_targetProfileEntry, false, false, 0);

        _applyButton = new Button(Translation.Get("trigger.add"));
        _applyButton.Clicked += (_, _) => ApplyTrigger();
        PackStart(_applyButton, false, false, 0);
    }

    private void ApplyTrigger()
    {
        string direction = _directionSelector.ActiveText?.Contains(">") == true ? "greater" : "less";
        string threshold = _thresholdEntry.Text;
        string target = _targetProfileEntry.Text;

        var payload = new JsonObject
        {
            ["source"] = _channelName,
            ["direction"] = direction,
            ["threshold"] = threshold,
            ["target"] = target
        };

        _rpc.SendRequest("createTrigger", payload);
    }

    private void Redraw()
    {
        BuildUi();
    }
}
