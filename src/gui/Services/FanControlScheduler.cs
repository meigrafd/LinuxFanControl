using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Timers;

namespace FanControl.Gui.Tools;

public class FanControlScheduler : VBox
{
    private readonly RpcClient _rpc;
    private Entry _timeEntry;
    private ComboBoxText _profileSelector;
    private Button _scheduleButton;
    private Label _statusLabel;
    private Timer _timer;

    public FanControlScheduler()
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

        _timeEntry = new Entry { PlaceholderText = "HH:mm" };
        PackStart(_timeEntry, false, false, 0);

        _profileSelector = new ComboBoxText();
        PackStart(_profileSelector, false, false, 0);

        _scheduleButton = new Button(Translation.Get("scheduler.set"));
        _scheduleButton.Clicked += (_, _) => ScheduleSwitch();
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
            foreach (var ch in channels)
            {
                var name = ch?["name"]?.ToString() ?? "";
                _profileSelector.AppendText(name);
            }

            if (_profileSelector.Children.Length > 0)
                _profileSelector.Active = 0;
        }
    }

    private void ScheduleSwitch()
    {
        var timeText = _timeEntry.Text;
        var profile = _profileSelector.ActiveText;

        if (TimeSpan.TryParse(timeText, out var targetTime) && !string.IsNullOrEmpty(profile))
        {
            _timer?.Stop();
            _timer = new Timer(1000);
            _timer.Elapsed += (_, _) =>
            {
                var now = DateTime.Now.TimeOfDay;
                if (Math.Abs((now - targetTime).TotalSeconds) < 1)
                {
                    var payload = new JsonObject { ["name"] = profile };
                    _rpc.SendRequest("setActiveProfile", payload);
                    _statusLabel.Text = Translation.Get("scheduler.done") + $" → {profile}";
                    _timer.Stop();
                }
            };
            _timer.Start();
            _statusLabel.Text = Translation.Get("scheduler.waiting") + $" {timeText}";
        }
        else
        {
            _statusLabel.Text = Translation.Get("scheduler.invalid");
        }
    }

    private void Redraw()
    {
        _scheduleButton.Label = Translation.Get("scheduler.set");
    }
}
