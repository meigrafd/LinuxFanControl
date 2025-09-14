using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class DaemonSelfTestRunner : VBox
{
    private readonly RpcClient _rpc;

    private Button _runButton;
    private TextView _resultView;

    public DaemonSelfTestRunner()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        _runButton = new Button(Translation.Get("selftest.run"));
        _runButton.Clicked += (_, _) => RunSelfTest();
        PackStart(_runButton, false, false, 0);

        _resultView = new TextView
        {
            Editable = false,
            WrapMode = WrapMode.Word
        };
        var scroll = new ScrolledWindow();
        scroll.Add(_resultView);
        scroll.SetSizeRequest(500, 250);
        PackStart(scroll, true, true, 0);
    }

    private void RunSelfTest()
    {
        var response = _rpc.SendRequest("runDaemonSelfTest");
        if (response is JsonObject obj && obj["result"] is JsonArray results)
        {
            if (results.Count == 0)
            {
                _resultView.Buffer.Text = Translation.Get("selftest.clean");
            }
            else
            {
                var lines = string.Join("\n", results.Select(r => "• " + r?.ToString()));
                _resultView.Buffer.Text = Translation.Get("selftest.issues") + "\n" + lines;
            }
        }
        else
        {
            _resultView.Buffer.Text = Translation.Get("selftest.error");
        }
    }

    private void Redraw()
    {
        _runButton.Label = Translation.Get("selftest.run");
    }
}
