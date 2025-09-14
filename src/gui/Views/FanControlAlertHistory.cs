using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using GLib;

namespace FanControl.Gui.Views;

public class FanControlAlertHistory : VBox
{
    private readonly RpcClient _rpc;
    private TreeView _alertView;
    private ListStore _alertStore;

    public FanControlAlertHistory()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadAlerts();
        Timeout.Add(10000, () =>
        {
            LoadAlerts();
            return true;
        });
    }

    private void BuildUi()
    {
        _alertStore = new ListStore(typeof(string), typeof(string), typeof(string));
        _alertView = new TreeView(_alertStore);

        var colTime = new TreeViewColumn { Title = Translation.Get("alert.time") };
        colTime.PackStart(new CellRendererText(), true);
        colTime.AddAttribute(colTime.CellRenderers[0], "text", 0);
        _alertView.AppendColumn(colTime);

        var colType = new TreeViewColumn { Title = Translation.Get("alert.type") };
        colType.PackStart(new CellRendererText(), true);
        colType.AddAttribute(colType.CellRenderers[0], "text", 1);
        _alertView.AppendColumn(colType);

        var colMessage = new TreeViewColumn { Title = Translation.Get("alert.message") };
        colMessage.PackStart(new CellRendererText(), true);
        colMessage.AddAttribute(colMessage.CellRenderers[0], "text", 2);
        _alertView.AppendColumn(colMessage);

        var scroll = new ScrolledWindow();
        scroll.Add(_alertView);
        scroll.SetSizeRequest(500, 250);
        PackStart(scroll, true, true, 0);
    }

    private void LoadAlerts()
    {
        _alertStore.Clear();

        var response = _rpc.SendRequest("getAlertHistory");
        if (response is JsonObject obj && obj["result"] is JsonArray alerts)
        {
            foreach (var a in alerts)
            {
                var time = a?["time"]?.ToString() ?? "?";
                var type = a?["type"]?.ToString() ?? "?";
                var msg = a?["message"]?.ToString() ?? "";
                _alertStore.AppendValues(time, type, msg);
            }
        }
    }

    private void Redraw()
    {
        LoadAlerts();
    }
}
