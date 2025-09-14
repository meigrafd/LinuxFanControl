using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Views;

public class DaemonSelfTestPanel : VBox
{
    private readonly RpcClient _rpc;

    private Button _runTestButton;
    private Label _resultLabel;

    public DaemonSelfTestPanel()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _runTestButton = new Button(Translation.Get("selftest.run"));
        _runTestButton.Clicked += (_, _) => RunSelfTest();
        PackStart(_runTestButton, false, false, 0);

        _resultLabel = new Label();
        PackStart(_resultLabel, false, false, 0);
    }

    private void RunSelfTest()
    {
        var response = _rpc.SendRequest("runSelfTest");
        if (response is JsonObject obj && obj["result"] is JsonObject result)
        {
            var ok = result["ok"]?.GetValue<bool>() ?? false;
            var msg = result["message"]?.ToString() ?? "";
            _resultLabel.Text = ok ? Translation.Get("selftest.ok") + $"\n{msg}" : Translation.Get("selftest.fail") + $"\n{msg}";
        }
        else
        {
            _resultLabel.Text = Translation.Get("selftest.error");
        }
    }

    private void Redraw()
    {
        _runTestButton.Label = Translation.Get("selftest.run");
    }
}
