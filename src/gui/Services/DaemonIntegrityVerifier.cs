using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class DaemonIntegrityVerifier : VBox
{
    private readonly RpcClient _rpc;

    private Button _verifyButton;
    private TextView _resultView;

    public DaemonIntegrityVerifier()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        _verifyButton = new Button(Translation.Get("integrity.run"));
        _verifyButton.Clicked += (_, _) => RunVerification();
        PackStart(_verifyButton, false, false, 0);

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

    private void RunVerification()
    {
        var response = _rpc.SendRequest("verifyDaemonIntegrity");
        if (response is JsonObject obj && obj["result"] is JsonArray issues)
        {
            if (issues.Count == 0)
            {
                _resultView.Buffer.Text = Translation.Get("integrity.clean");
            }
            else
            {
                var lines = string.Join("\n", issues.Select(i => "• " + i?.ToString()));
                _resultView.Buffer.Text = Translation.Get("integrity.issues") + "\n" + lines;
            }
        }
        else
        {
            _resultView.Buffer.Text = Translation.Get("integrity.error");
        }
    }

    private void Redraw()
    {
        _verifyButton.Label = Translation.Get("integrity.run");
    }
}
