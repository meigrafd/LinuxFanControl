using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using GLib;

namespace FanControl.Gui.Views;

public class FanControlEventTracer : VBox
{
    private readonly RpcClient _rpc;
    private TreeView _eventView;
    private ListStore _eventStore;

    public FanControlEventTracer()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadEvents();
        Timeout.Add(3000, () =>
        {
            LoadEvents();
            return true;
        });
    }

    private void BuildUi()
    {
        _eventStore = new ListStore(typeof(string), typeof(string), typeof(string));
        _eventView = new TreeView(_eventStore);

        var colTime = new TreeViewColumn { Title = Translation.Get("trace.time") };
        colTime.PackStart(new CellRendererText(), true);
        colTime.AddAttribute(colTime.CellRenderers[0], "text", 0);
        _eventView.AppendColumn(colTime);

        var colType = new TreeViewColumn { Title = Translation.Get("trace.type") };
        colType.PackStart(new CellRendererText(), true);
        colType.AddAttribute(colType.CellRenderers[0], "text", 1);
        _eventView.AppendColumn(colType);

        var colDetail = new TreeViewColumn { Title = Translation.Get("trace.detail") };
        colDetail.PackStart(new CellRendererText(), true);
        colDetail.AddAttribute(colDetail.CellRenderers[0], "text", 2);
        _eventView.AppendColumn(colDetail);

        var scroll = new ScrolledWindow();
        scroll.Add(_eventView);
        scroll.SetSizeRequest(500, 250);
        PackStart(scroll, true, true, 0);
    }

    private void LoadEvents()
    {
        _eventStore.Clear();

        var response = _rpc.SendRequest("getRecentEvents");
        if (response is JsonObject obj && obj["result"] is JsonArray events)
        {
            foreach (var e in events)
            {
                var time = e?["time"]?.ToString() ?? "?";
                var type = e?["type"]?.ToString() ?? "?";
                var detail = e?["detail"]?.ToString() ?? "";
                _eventStore.AppendValues(time, type, detail);
            }
        }
    }

    private void Redraw()
    {
        LoadEvents();
    }
}
