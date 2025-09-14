using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlRuleProfiler : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _ruleSelector;
    private Button _profileButton;
    private TextView _resultView;

    public FanControlRuleProfiler()
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

        _profileButton = new Button(Translation.Get("ruleprofile.run"));
        _profileButton.Clicked += (_, _) => ProfileRule();
        PackStart(_profileButton, false, false, 0);

        _resultView = new TextView
        {
            Editable = false,
            WrapMode = WrapMode.Word
        };
        var scroll = new ScrolledWindow();
        scroll.Add(_resultView);
        scroll.SetSizeRequest(500, 200);
        PackStart(scroll, true, true, 0);
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

    private void ProfileRule()
    {
        var rule = _ruleSelector.ActiveText;
        if (!string.IsNullOrEmpty(rule))
        {
            var payload = new JsonObject { ["name"] = rule };
            var response = _rpc.SendRequest("profileFanRule", payload);
            if (response is JsonObject obj && obj["result"] is JsonObject result)
            {
                var evals = result["evaluations"]?.ToString() ?? "?";
                var avgTime = result["avg_time_ms"]?.ToString() ?? "?";
                var maxTime = result["max_time_ms"]?.ToString() ?? "?";
                _resultView.Buffer.Text = Translation.Get("ruleprofile.result") +
                $"\nEvaluations: {evals}\nAvg Time: {avgTime} ms\nMax Time: {maxTime} ms";
            }
            else
            {
                _resultView.Buffer.Text = Translation.Get("ruleprofile.error");
            }
        }
        else
        {
            _resultView.Buffer.Text = Translation.Get("ruleprofile.invalid");
        }
    }

    private void Redraw()
    {
        _profileButton.Label = Translation.Get("ruleprofile.run");
    }
}
