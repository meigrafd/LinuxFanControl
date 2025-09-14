using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlRuleCompiler : VBox
{
    private readonly RpcClient _rpc;

    private TextView _ruleEditor;
    private Button _compileButton;
    private TextView _resultView;

    public FanControlRuleCompiler()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        _ruleEditor = new TextView
        {
            WrapMode = WrapMode.Word,
            Buffer = new TextBuffer(new TextTagTable())
        };
        var scrollEditor = new ScrolledWindow();
        scrollEditor.Add(_ruleEditor);
        scrollEditor.SetSizeRequest(500, 150);
        PackStart(scrollEditor, true, true, 0);

        _compileButton = new Button(Translation.Get("rule.compile"));
        _compileButton.Clicked += (_, _) => CompileRules();
        PackStart(_compileButton, false, false, 0);

        _resultView = new TextView
        {
            Editable = false,
            WrapMode = WrapMode.Word
        };
        var scrollResult = new ScrolledWindow();
        scrollResult.Add(_resultView);
        scrollResult.SetSizeRequest(500, 150);
        PackStart(scrollResult, true, true, 0);
    }

    private void CompileRules()
    {
        var ruleText = _ruleEditor.Buffer.Text;
        if (!string.IsNullOrWhiteSpace(ruleText))
        {
            var payload = new JsonObject { ["rules"] = ruleText };
            var response = _rpc.SendRequest("compileFanRules", payload);
            if (response is JsonObject obj && obj["result"] is JsonArray messages)
            {
                if (messages.Count == 0)
                {
                    _resultView.Buffer.Text = Translation.Get("rule.clean");
                }
                else
                {
                    var lines = string.Join("\n", messages.Select(m => "• " + m?.ToString()));
                    _resultView.Buffer.Text = Translation.Get("rule.issues") + "\n" + lines;
                }
            }
            else
            {
                _resultView.Buffer.Text = Translation.Get("rule.error");
            }
        }
        else
        {
            _resultView.Buffer.Text = Translation.Get("rule.invalid");
        }
    }

    private void Redraw()
    {
        _compileButton.Label = Translation.Get("rule.compile");
    }
}
