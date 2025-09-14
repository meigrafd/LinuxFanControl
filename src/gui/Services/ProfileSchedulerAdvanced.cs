using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace FanControl.Gui.Tools;

public class ProfileSchedulerAdvanced : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _profileSelector;
    private Entry _timeEntry;
    private ComboBoxText _conditionSelector;
    private Entry _thresholdEntry;
    private Button _scheduleButton;
    private Label _statusLabel;

    public ProfileSchedulerAdvanced()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadProfiles();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _profileSelector = new ComboBoxText();
        PackStart(_profileSelector, false, false, 0);

        _timeEntry = new Entry { PlaceholderText = "HH:mm (optional)" };
        PackStart(_timeEntry, false, false, 0);

        _conditionSelector = new ComboBoxText();
        _conditionSelector.AppendText("Sensor above");
        _conditionSelector.AppendText("Sensor below");
        _conditionSelector.Active = 0;
        PackStart(_conditionSelector, false, false, 0);

        _thresholdEntry = new Entry { PlaceholderText = "Threshold (e.g. 65.0)" };
        PackStart(_thresholdEntry, false, false, 0);

        _scheduleButton = new Button(Translation.Get("scheduler.advanced.set"));
        _scheduleButton.Clicked += (_, _) => ScheduleAdvanced();
        PackStart(_scheduleButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadProfiles()
    {
        _profileSelector.RemoveAll();

        var response = _rpc.SendRequest("listChannels");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            var names = new HashSet<string>();
            foreach (var ch in channels)
            {
                var name = ch?["name"]?.ToString() ?? "";
                if (!string.IsNullOrEmpty(name) && !names.Contains(name))
                {
                    _profileSelector.AppendText(name);
                    names.Add(name);
                }
            }

            if (_profileSelector.Children.Length > 0)
                _profileSelector.Active = 0;
        }
    }

    private void ScheduleAdvanced()
    {
        var profile = _profileSelector.ActiveText;
        var timeText = _timeEntry.Text;
        var condition = _conditionSelector.ActiveText;
        var thresholdText = _thresholdEntry.Text;

        if (!string.IsNullOrEmpty(profile) && double.TryParse(thresholdText, out var threshold))
        {
            var payload = new JsonObject
            {
                ["profile"] = profile,
                ["condition"] = condition,
                ["threshold"] = threshold,
                ["time"] = timeText
            };
            _rpc.SendRequest("scheduleProfileAdvanced", payload);
            _statusLabel.Text = Translation.Get("scheduler.advanced.success");
        }
        else
        {
            _statusLabel.Text = Translation.Get("scheduler.advanced.invalid");
        }
    }

    private void Redraw()
    {
        _scheduleButton.Label = Translation.Get("scheduler.advanced.set");
    }
}
