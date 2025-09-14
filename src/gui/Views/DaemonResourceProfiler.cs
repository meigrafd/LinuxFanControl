using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using GLib;

namespace FanControl.Gui.Views;

public class DaemonResourceProfiler : VBox
{
    private readonly RpcClient _rpc;
    private TreeView _resourceView;
    private ListStore _resourceStore;

    public DaemonResourceProfiler()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        UpdateResources();
        Timeout.Add(4000, () =>
        {
            UpdateResources();
            return true;
        });
    }

    private void BuildUi()
    {
        _resourceStore = new ListStore(typeof(string), typeof(string), typeof(string));
        _resourceView = new TreeView(_resourceStore);

        var colModule = new TreeViewColumn { Title = Translation.Get("resource.module") };
        colModule.PackStart(new CellRendererText(), true);
        colModule.AddAttribute(colModule.CellRenderers[0], "text", 0);
        _resourceView.AppendColumn(colModule);

        var colCpu = new TreeViewColumn { Title = Translation.Get("resource.cpu") };
        colCpu.PackStart(new CellRendererText(), true);
        colCpu.AddAttribute(colCpu.CellRenderers[0], "text", 1);
        _resourceView.AppendColumn(colCpu);

        var colMem = new TreeViewColumn { Title = Translation.Get("resource.memory") };
        colMem.PackStart(new CellRendererText(), true);
        colMem.AddAttribute(colMem.CellRenderers[0], "text", 2);
        _resourceView.AppendColumn(colMem);

        var scroll = new ScrolledWindow();
        scroll.Add(_resourceView);
        scroll.SetSizeRequest(500, 200);
        PackStart(scroll, true, true, 0);
    }

    private void UpdateResources()
    {
        _resourceStore.Clear();

        var response = _rpc.SendRequest("getModuleResources");
        if (response is JsonObject obj && obj["result"] is JsonArray modules)
        {
            foreach (var m in modules)
            {
                var name = m?["name"]?.ToString() ?? "?";
                var cpu = m?["cpu"]?.ToString() ?? "?";
                var mem = m?["memory"]?.ToString() ?? "?";
                _resourceStore.AppendValues(name, cpu + " %", mem + " MB");
            }
        }
    }

    private void Redraw()
    {
        UpdateResources();
    }
}
