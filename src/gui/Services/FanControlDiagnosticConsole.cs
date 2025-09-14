using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlDiagnosticConsole : VBox
{
    private readonly RpcClient _rpc;

    private Entry _methodEntry;
    private TextView _payloadView;
    private Button _sendButton;
    private TextView _resultView;

    public FanControlDiagnosticConsole()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        _methodEntry = new Entry { PlaceholderText = Translation.Get("console.method") };
        PackStart(_methodEntry, false, false, 0);

        _payloadView = new TextView
        {
            WrapMode = WrapMode.Word,
            Buffer = new TextBuffer(new TextTagTable())
        };
        var scrollPayload = new ScrolledWindow();
        scrollPayload.Add(_payloadView);
        scrollPayload.SetSizeRequest(400, 100);
        PackStart(scrollPayload, true, true, 0);

        _sendButton = new Button(Translation.Get("console.send"));
        _sendButton.Clicked += (_, _) => SendRpc();
        PackStart(_sendButton, false, false, 0);

        _resultView = new TextView
        {
            Editable = false,
            WrapMode = WrapMode.Word
        };
        var scrollResult = new ScrolledWindow();
        scrollResult.Add(_resultView);
        scrollResult.SetSizeRequest(400, 150);
        PackStart(scrollResult, true, true, 0);
    }

    private void SendRpc()
    {
        var method = _methodEntry.Text;
        var payloadText = _payloadView.Buffer.Text;

        try
        {
            var payload = string.IsNullOrWhiteSpace(payloadText)
            ? null
            : JsonNode.Parse(payloadText);

            var response = _rpc.SendRequest(method, payload);
            _resultView.Buffer.Text = response?.ToJsonString(new System.Text.Json.JsonSerializerOptions
            {
                WriteIndented = true
            }) ?? Translation.Get("console.empty");
        }
        catch
        {
            _resultView.Buffer.Text = Translation.Get("console.error");
        }
    }

    private void Redraw()
    {
        _sendButton.Label = Translation.Get("console.send");
    }
}
