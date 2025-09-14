using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using GLib;

namespace FanControl.Gui.Views;

public class ChannelAuditTrail : ScrolledWindow
{
    private readonly RpcClient _rpc;
    private TextView _logView;
    private string _channelName;

    public ChannelAuditTrail(string channelName)
    {
        _channelName = channelName;
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

        LoadAudit();
        Timeout.Add(5000, () =>
        {
            LoadAudit();
            return true;
        });
    }

    private void LoadAudit()
    {
        var payload = new JsonObject { ["name"] = _channelName };
        var response = _rpc.SendRequest("getChannelAudit", payload);
        if (response is JsonObject obj && obj["result"] is JsonArray entries)
        {
            var lines = new List<string>();
            foreach (var e in entries)
            {
                var ts = e?["timestamp"]?.ToString() ?? "?";
                var action = e?["action"]?.ToString() ?? "change";
                var detail = e?["detail"]?.ToString() ?? "";
                lines.Add($"{ts} [{action}] {detail}");
            }

            _logView.Buffer.Text = string.Join("\n", lines.TakeLast(100));
        }
        else
        {
            _logView.Buffer.Text = Translation.Get("audit.error");
        }
    }

    private void Redraw()
    {
        LoadAudit();
    }
}
