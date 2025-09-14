using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlAutoTuner : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _channelSelector;
    private Button _startButton;
    private Label _resultLabel;

    public FanControlAutoTuner()
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

        _startButton = new Button(Translation.Get("autotune.start"));
        _startButton.Clicked += (_, _) => RunAutoTune();
        PackStart(_startButton, false, false, 0);

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

    private void RunAutoTune()
    {
        var channel = _channelSelector.ActiveText;
        if (!string.IsNullOrEmpty(channel))
        {
            var payload = new JsonObject { ["name"] = channel };
            var response = _rpc.SendRequest("runAutoTuning", payload);
            if (response is JsonObject obj && obj["result"] is JsonObject result)
            {
                var profile = result["profile"]?.ToString() ?? "?";
                var score = result["score"]?.ToString() ?? "?";
                _resultLabel.Text = Translation.Get("autotune.result") + $"\nProfile: {profile}\nScore: {score}";
            }
            else
            {
                _resultLabel.Text = Translation.Get("autotune.error");
            }
        }
        else
        {
            _resultLabel.Text = Translation.Get("autotune.invalid");
        }
    }

    private void Redraw()
    {
        _startButton.Label = Translation.Get("autotune.start");
    }
}
