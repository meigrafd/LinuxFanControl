using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using GLib;

namespace FanControl.Gui.Views;

public class FanControlEventLog : ScrolledWindow
{
    private readonly RpcClient _rpc;
    private TextView _logView;

    public FanControlEventLog()
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

        LoadEvents();
        Timeout.Add(5000, () =>
        {
            LoadEvents();
            return true;
        });
    }

    private void LoadEvents()
    {
        var response = _rpc.SendRequest("getEventLog");
        if (response is JsonObject obj && obj["result"] is JsonArray events)
        {
            var lines = new List<string>();
            foreach (var e in events)
            {
                var ts = e?["timestamp"]?.ToString() ?? "?";
                var type = e?["type"]?.ToString() ?? "event";
                var msg = e?["message"]?.ToString() ?? "";
                lines.Add($"{ts} [{type}] {msg}");
            }

            _logView.Buffer.Text = string.Join("\n", lines.TakeLast(100));
        }
        else
        {
            _logView.Buffer.Text = Translation.Get("eventlog.error");
        }
    }

    private void Redraw()
    {
        LoadEvents();
    }
}
