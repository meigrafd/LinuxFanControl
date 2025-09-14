using Gtk;
using FanControl.Gui.Services;

namespace FanControl.Gui.Widgets;

public class StatusBanner : EventBox
{
    private Label _statusLabel;
    private RpcClient _rpc;

    public StatusBanner()
    {
        _rpc = new RpcClient();
        _statusLabel = new Label();
        Add(_statusLabel);

        Translation.LanguageChanged += Redraw;

        UpdateStatus();
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
