using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using GLib;

namespace FanControl.Gui.Views;

public class FanControlLoadMonitor : VBox
{
    private readonly RpcClient _rpc;
    private TreeView _loadView;
    private ListStore _loadStore;

    public FanControlLoadMonitor()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        UpdateLoad();
        Timeout.Add(5000, () =>
        {
            UpdateLoad();
            return true;
        });
    }

    private void BuildUi()
    {
        _loadStore = new ListStore(typeof(string), typeof(string), typeof(string));
        _loadView = new TreeView(_loadStore);

        var colChannel = new TreeViewColumn { Title = Translation.Get("load.channel") };
        colChannel.PackStart(new CellRendererText(), true);
        colChannel.AddAttribute(colChannel.CellRenderers[0], "text", 0);
        _loadView.AppendColumn(colChannel);

        var colPwm = new TreeViewColumn { Title = Translation.Get("load.pwm") };
        colPwm.PackStart(new CellRendererText(), true);
        colPwm.AddAttribute(colPwm.CellRenderers[0], "text", 1);
        _loadView.AppendColumn(colPwm);

        var colRule = new TreeViewColumn { Title = Translation.Get("load.rule") };
        colRule.PackStart(new CellRendererText(), true);
        colRule.AddAttribute(colRule.CellRenderers[0], "text", 2);
        _loadView.AppendColumn(colRule);

        var scroll = new ScrolledWindow();
        scroll.Add(_loadView);
        scroll.SetSizeRequest(500, 250);
        PackStart(scroll, true, true, 0);
    }

    private void UpdateLoad()
    {
        _loadStore.Clear();

        var response = _rpc.SendRequest("getChannelLoadStatus");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            foreach (var ch in channels)
            {
                var name = ch?["name"]?.ToString() ?? "?";
                var pwm = ch?["pwm"]?.ToString() ?? "?";
                var rule = ch?["active_rule"]?.ToString() ?? "-";
                _loadStore.AppendValues(name, pwm + " %", rule);
            }
        }
    }

    private void Redraw()
    {
        UpdateLoad();
    }
}
