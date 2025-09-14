using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlRuleOptimizer : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _ruleSelector;
    private Button _optimizeButton;
    private TextView _resultView;

    public FanControlRuleOptimizer()
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

        _optimizeButton = new Button(Translation.Get("optimize.run"));
        _optimizeButton.Clicked += (_, _) => OptimizeRule();
        PackStart(_optimizeButton, false, false, 0);

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

    private void OptimizeRule()
    {
        var rule = _ruleSelector.ActiveText;
        if (!string.IsNullOrEmpty(rule))
        {
            var payload = new JsonObject { ["name"] = rule };
            var response = _rpc.SendRequest("optimizeFanRule", payload);
            if (response is JsonObject obj && obj["result"] is JsonArray suggestions)
            {
                if (suggestions.Count == 0)
                {
                    _resultView.Buffer.Text = Translation.Get("optimize.clean");
                }
                else
                {
                    var lines = string.Join("\n", suggestions.Select(s => "• " + s?.ToString()));
                    _resultView.Buffer.Text = Translation.Get("optimize.result") + "\n" + lines;
                }
            }
            else
            {
                _resultView.Buffer.Text = Translation.Get("optimize.error");
            }
        }
        else
        {
            _resultView.Buffer.Text = Translation.Get("optimize.invalid");
        }
    }

    private void Redraw()
    {
        _optimizeButton.Label = Translation.Get("optimize.run");
    }
}
