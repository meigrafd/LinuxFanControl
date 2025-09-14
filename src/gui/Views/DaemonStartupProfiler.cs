using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Views;

public class DaemonStartupProfiler : VBox
{
    private readonly RpcClient _rpc;
    private TreeView _phaseView;
    private ListStore _phaseStore;

    public DaemonStartupProfiler()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadStartupPhases();
    }

    private void BuildUi()
    {
        _phaseStore = new ListStore(typeof(string), typeof(string));
        _phaseView = new TreeView(_phaseStore);

        var colPhase = new TreeViewColumn { Title = Translation.Get("startup.phase") };
        colPhase.PackStart(new CellRendererText(), true);
        colPhase.AddAttribute(colPhase.CellRenderers[0], "text", 0);
        _phaseView.AppendColumn(colPhase);

        var colDuration = new TreeViewColumn { Title = Translation.Get("startup.duration") };
        colDuration.PackStart(new CellRendererText(), true);
        colDuration.AddAttribute(colDuration.CellRenderers[0], "text", 1);
        _phaseView.AppendColumn(colDuration);

        var scroll = new ScrolledWindow();
        scroll.Add(_phaseView);
        scroll.SetSizeRequest(500, 200);
        PackStart(scroll, true, true, 0);
    }

    private void LoadStartupPhases()
    {
        _phaseStore.Clear();

        var response = _rpc.SendRequest("getStartupProfile");
        if (response is JsonObject obj && obj["result"] is JsonArray phases)
        {
            foreach (var p in phases)
            {
                var name = p?["name"]?.ToString() ?? "?";
                var time = p?["duration_ms"]?.ToString() ?? "?";
                _phaseStore.AppendValues(name, time + " ms");
            }
        }
    }

    private void Redraw()
    {
        LoadStartupPhases();
    }
}
