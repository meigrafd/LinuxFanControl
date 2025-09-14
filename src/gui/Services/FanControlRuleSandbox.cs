using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlRuleSandbox : VBox
{
    private readonly RpcClient _rpc;

    private TextView _ruleEditor;
    private Button _testButton;
    private TextView _resultView;

    public FanControlRuleSandbox()
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

        _testButton = new Button(Translation.Get("sandbox.run"));
        _testButton.Clicked += (_, _) => RunSandbox();
        PackStart(_testButton, false, false, 0);

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

    private void RunSandbox()
    {
        var ruleText = _ruleEditor.Buffer.Text;
        if (!string.IsNullOrWhiteSpace(ruleText))
        {
            var payload = new JsonObject { ["rule"] = ruleText };
            var response = _rpc.SendRequest("testRuleInSandbox", payload);
            if (response is JsonObject obj && obj["result"] is JsonArray output)
            {
                var lines = string.Join("\n", output.Select(o => "• " + o?.ToString()));
                _resultView.Buffer.Text = Translation.Get("sandbox.result") + "\n" + lines;
            }
            else
            {
                _resultView.Buffer.Text = Translation.Get("sandbox.error");
            }
        }
        else
        {
            _resultView.Buffer.Text = Translation.Get("sandbox.invalid");
        }
    }

    private void Redraw()
    {
        _testButton.Label = Translation.Get("sandbox.run");
    }
}
