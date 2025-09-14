using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlScenarioPlanner : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _channelSelector;
    private Entry _tempEntry;
    private Button _simulateButton;
    private Label _resultLabel;

    public FanControlScenarioPlanner()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadChannels();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _channelSelector = new ComboBoxText();
        PackStart(_channelSelector, false, false, 0);

        _tempEntry = new Entry { PlaceholderText = Translation.Get("scenario.temp") };
        PackStart(_tempEntry, false, false, 0);

        _simulateButton = new Button(Translation.Get("scenario.run"));
        _simulateButton.Clicked += (_, _) => RunSimulation();
        PackStart(_simulateButton, false, false, 0);

        _resultLabel = new Label();
        PackStart(_resultLabel, false, false, 0);
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

    private void RunSimulation()
    {
        var channel = _channelSelector.ActiveText;
        var tempText = _tempEntry.Text;

        if (!string.IsNullOrEmpty(channel) && double.TryParse(tempText, out var temp))
        {
            var payload = new JsonObject
            {
                ["name"] = channel,
                ["temperature"] = temp
            };
            var response = _rpc.SendRequest("simulateChannelResponse", payload);
            if (response is JsonObject obj && obj["result"] is JsonObject result)
            {
                var pwm = result["pwm"]?.ToString() ?? "?";
                _resultLabel.Text = Translation.Get("scenario.result") + $" → {pwm}%";
            }
            else
            {
                _resultLabel.Text = Translation.Get("scenario.error");
            }
        }
        else
        {
            _resultLabel.Text = Translation.Get("scenario.invalid");
        }
    }

    private void Redraw()
    {
        _simulateButton.Label = Translation.Get("scenario.run");
    }
}
