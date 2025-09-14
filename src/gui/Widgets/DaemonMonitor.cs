using Gtk;
using GLib;
using FanControl.Gui.Services;

namespace FanControl.Gui.Widgets;

public class DaemonMonitor : VBox
{
    private readonly RpcClient _rpc;
    private Label _statusLabel;
    private uint _refreshId;

    public DaemonMonitor()
    {
        Spacing = 6;
        _rpc = new RpcClient();

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);

        Translation.LanguageChanged += Redraw;

        UpdateStatus();
        _refreshId = Timeout.Add(3000, new TimeoutHandler(() =>
        {
            UpdateStatus();
            return true;
        }));
    }

    private void UpdateStatus()
    {
        bool pidExists = System.IO.File.Exists("/run/pid/lfcd.pid");
        bool responds = _rpc.Ping();

        string key = "daemon.status.unknown";
        if (pidExists && responds)
            key = "daemon.status.running";
        else if (pidExists && !responds)
            key = "daemon.status.unresponsive";
        else if (!pidExists && responds)
            key = "daemon.status.nopid";
        else
            key = "daemon.status.dead";

        _statusLabel.Text = Translation.Get(key);
    }

    private void Redraw()
    {
        UpdateStatus();
    }
}
