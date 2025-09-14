using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlRuleAuditor : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _ruleSelector;
    private Button _auditButton;
    private TextView _resultView;

    public FanControlRuleAuditor()
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

        _auditButton = new Button(Translation.Get("audit.run"));
        _auditButton.Clicked += (_, _) => AuditRule();
        PackStart(_auditButton, false, false, 0);

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

    private void AuditRule()
    {
        var rule = _ruleSelector.ActiveText;
        if (!string.IsNullOrEmpty(rule))
        {
            var payload = new JsonObject { ["name"] = rule };
            var response = _rpc.SendRequest("auditFanRule", payload);
            if (response is JsonObject obj && obj["result"] is JsonArray findings)
            {
                if (findings.Count == 0)
                {
                    _resultView.Buffer.Text = Translation.Get("audit.clean");
                }
                else
                {
                    var lines = string.Join("\n", findings.Select(f => "• " + f?.ToString()));
                    _resultView.Buffer.Text = Translation.Get("audit.issues") + "\n" + lines;
                }
            }
            else
            {
                _resultView.Buffer.Text = Translation.Get("audit.error");
            }
        }
        else
        {
            _resultView.Buffer.Text = Translation.Get("audit.invalid");
        }
    }

    private void Redraw()
    {
        _auditButton.Label = Translation.Get("audit.run");
    }
}
