using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlScheduleEditor : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _profileSelector;
    private Entry _timeEntry;
    private Button _addButton;
    private Label _statusLabel;

    public FanControlScheduleEditor()
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

        _addButton = new Button(Translation.Get("schedule.add"));
        _addButton.Clicked += (_, _) => AddSchedule();
        PackStart(_addButton, false, false, 0);

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

    private void AddSchedule()
    {
        var profile = _profileSelector.ActiveText;
        var time = _timeEntry.Text;

        if (!string.IsNullOrEmpty(profile) && TimeSpan.TryParse(time, out var parsed))
        {
            var payload = new JsonObject
            {
                ["profile"] = profile,
                ["time"] = time
            };
            var response = _rpc.SendRequest("addProfileSchedule", payload);
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
        _addButton.Label = Translation.Get("schedule.add");
    }
}
