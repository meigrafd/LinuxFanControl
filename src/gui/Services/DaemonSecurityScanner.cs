using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class DaemonSecurityScanner : VBox
{
    private readonly RpcClient _rpc;

    private Button _scanButton;
    private Label _resultLabel;

    public DaemonSecurityScanner()
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

        _scanButton = new Button(Translation.Get("security.scan"));
        _scanButton.Clicked += (_, _) => RunScan();
        PackStart(_scanButton, false, false, 0);

        _resultLabel = new Label();
        PackStart(_resultLabel, false, false, 0);
    }

    private void RunScan()
    {
        var response = _rpc.SendRequest("runSecurityScan");
        if (response is JsonObject obj && obj["result"] is JsonArray findings)
        {
            if (findings.Count == 0)
            {
                _resultLabel.Text = Translation.Get("security.clean");
            }
            else
            {
                var lines = string.Join("\n", findings.Select(f => "• " + f?.ToString()));
                _resultLabel.Text = Translation.Get("security.issues") + "\n" + lines;
            }
        }
        else
        {
            _resultLabel.Text = Translation.Get("security.error");
        }
    }

    private void Redraw()
    {
        _scanButton.Label = Translation.Get("security.scan");
    }
}
