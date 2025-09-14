using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlPowerScheduler : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _channelSelector;
    private Entry _startTimeEntry;
    private Entry _endTimeEntry;
    private Entry _maxPowerEntry;
    private Button _scheduleButton;
    private Label _statusLabel;

    public FanControlPowerScheduler()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadChannels();
    }

    private void BuildUi()
    {
        _channelSelector = new ComboBoxText();
        PackStart(_channelSelector, false, false, 0);

        _startTimeEntry = new Entry { PlaceholderText = Translation.Get("power.start") };
        PackStart(_startTimeEntry, false, false, 0);

        _endTimeEntry = new Entry { PlaceholderText = Translation.Get("power.end") };
        PackStart(_endTimeEntry, false, false, 0);

        _maxPowerEntry = new Entry { PlaceholderText = Translation.Get("power.limit") };
        PackStart(_maxPowerEntry, false, false, 0);

        _scheduleButton = new Button(Translation.Get("power.schedule"));
        _scheduleButton.Clicked += (_, _) => ApplySchedule();
        PackStart(_scheduleButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadChannels()
    {
        _channelSelector.RemoveAll();

        var response = _rpc.SendRequest("listChannels");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            foreach (var ch in channels)
            {
                var name = ch?["name"]?.ToString() ?? "";
                _channelSelector.AppendText(name);
            }

            if (_channelSelector.Children.Length > 0)
                _channelSelector.Active = 0;
        }
    }

    private void ApplySchedule()
    {
        var channel = _channelSelector.ActiveText;
        var start = _startTimeEntry.Text;
        var end = _endTimeEntry.Text;
        var limitText = _maxPowerEntry.Text;

        if (!string.IsNullOrEmpty(channel) &&
            TimeSpan.TryParse(start, out var _) &&
            TimeSpan.TryParse(end, out var _) &&
            double.TryParse(limitText, out var limit))
        {
            var payload = new JsonObject
            {
                ["channel"] = channel,
                ["start"] = start,
                ["end"] = end,
                ["maxPower"] = limit
            };
            var response = _rpc.SendRequest("schedulePowerLimit", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("power.success")
            : Translation.Get("power.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("power.invalid");
        }
    }

    private void Redraw()
    {
        _scheduleButton.Label = Translation.Get("power.schedule");
    }
}
