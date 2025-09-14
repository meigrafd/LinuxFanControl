using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlTerminalBridge : VBox
{
    private readonly RpcClient _rpc;

    private Entry _commandEntry;
    private Button _sendButton;
    private TextView _outputView;

    public FanControlTerminalBridge()
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

        _commandEntry = new Entry { PlaceholderText = Translation.Get("terminal.command") };
        PackStart(_commandEntry, false, false, 0);

        _sendButton = new Button(Translation.Get("terminal.send"));
        _sendButton.Clicked += (_, _) => SendCommand();
        PackStart(_sendButton, false, false, 0);

        _outputView = new TextView
        {
            Editable = false,
            WrapMode = WrapMode.Word
        };
        var scroll = new ScrolledWindow();
        scroll.Add(_outputView);
        scroll.SetPolicy(PolicyType.Automatic, PolicyType.Automatic);
        scroll.SetSizeRequest(400, 200);
        PackStart(scroll, true, true, 0);
    }

    private void SendCommand()
    {
        var cmd = _commandEntry.Text;
        if (!string.IsNullOrEmpty(cmd))
        {
            var payload = new JsonObject { ["command"] = cmd };
            var response = _rpc.SendRequest("terminalExec", payload);
            if (response is JsonObject obj && obj["result"] is JsonValue result)
            {
                _outputView.Buffer.Text = result.ToString();
            }
            else
            {
                _outputView.Buffer.Text = Translation.Get("terminal.error");
            }
        }
    }

    private void Redraw()
    {
        _sendButton.Label = Translation.Get("terminal.send");
    }
}
