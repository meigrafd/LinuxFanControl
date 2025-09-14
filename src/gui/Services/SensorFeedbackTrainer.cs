using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class SensorFeedbackTrainer : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _ruleSelector;
    private Button _trainButton;
    private Label _statusLabel;

    public SensorFeedbackTrainer()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadRules();
    }

    private void BuildUi()
    {
        _ruleSelector = new ComboBoxText();
        PackStart(_ruleSelector, false, false, 0);

        _trainButton = new Button(Translation.Get("feedback.train"));
        _trainButton.Clicked += (_, _) => TrainRule();
        PackStart(_trainButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadRules()
    {
        _ruleSelector.RemoveAll();

        var response = _rpc.SendRequest("listFanRules");
        if (response is JsonObject obj && obj["result"] is JsonArray rules)
        {
            foreach (var r in rules)
            {
                var name = r?.ToString() ?? "?";
                _ruleSelector.AppendText(name);
            }

            if (_ruleSelector.Children.Length > 0)
                _ruleSelector.Active = 0;
        }
    }

    private void TrainRule()
    {
        var rule = _ruleSelector.ActiveText;
        if (!string.IsNullOrEmpty(rule))
        {
            var payload = new JsonObject { ["name"] = rule };
            var response = _rpc.SendRequest("trainRuleWithFeedback", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("feedback.success")
            : Translation.Get("feedback.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("feedback.invalid");
        }
    }

    private void Redraw()
    {
        _trainButton.Label = Translation.Get("feedback.train");
    }
}
