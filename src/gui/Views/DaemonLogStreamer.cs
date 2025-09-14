using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using GLib;

namespace FanControl.Gui.Views;

public class DaemonLogStreamer : ScrolledWindow
{
    private readonly RpcClient _rpc;
    private TextView _logView;

    public DaemonLogStreamer()
    {
        _rpc = new RpcClient();
        SetPolicy(PolicyType.Automatic, PolicyType.Automatic);
        BorderWidth = 6;

        _logView = new TextView
        {
            Editable = false,
            WrapMode = WrapMode.Word
        };

        Add(_logView);

        Translation.LanguageChanged += Redraw;

        StreamLogs();
        Timeout.Add(3000, () =>
        {
            StreamLogs();
            return true;
        });
    }

    private void StreamLogs()
    {
        var response = _rpc.SendRequest("getDaemonLogTail");
        if (response is JsonObject obj && obj["result"] is JsonArray lines)
        {
            var text = string.Join("\n", lines.Select(l => l?.ToString() ?? ""));
            _logView.Buffer.Text = text;
        }
        else
        {
            _logView.Buffer.Text = Translation.Get("log.error");
        }
    }

    private void Redraw()
    {
        StreamLogs();
    }
}
