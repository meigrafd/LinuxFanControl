using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlSchedulePlanner : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _profileSelector;
    private Entry _timeEntry;
    private Button _scheduleButton;
    private Label _statusLabel;

    public FanControlSchedulePlanner()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadProfiles();
    }

    private void BuildUi()
    {
        _profileSelector = new ComboBoxText();
        PackStart(_profileSelector, false, false, 0);

        _timeEntry = new Entry { PlaceholderText = Translation.Get("schedule.time") };
        PackStart(_timeEntry, false, false, 0);

        _scheduleButton = new Button(Translation.Get("schedule.set"));
        _scheduleButton.Clicked += (_, _) => ScheduleProfile();
        PackStart(_scheduleButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadProfiles()
    {
        _profileSelector.RemoveAll();

        var response = _rpc.SendRequest("listProfiles");
        if (response is JsonObject obj && obj["result"] is JsonArray profiles)
        {
            foreach (var p in profiles)
            {
                var name = p?.ToString() ?? "?";
                _profileSelector.AppendText(name);
            }

            if (_profileSelector.Children.Length > 0)
                _profileSelector.Active = 0;
        }
    }

    private void ScheduleProfile()
    {
        var profile = _profileSelector.ActiveText;
        var time = _timeEntry.Text;

        if (!string.IsNullOrEmpty(profile) && !string.IsNullOrEmpty(time))
        {
            var payload = new JsonObject
            {
                ["profile"] = profile,
                ["time"] = time
            };
            var response = _rpc.SendRequest("scheduleFanProfile", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("schedule.success")
            : Translation.Get("schedule.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("schedule.invalid");
        }
    }

    private void Redraw()
    {
        _scheduleButton.Label = Translation.Get("schedule.set");
    }
}
