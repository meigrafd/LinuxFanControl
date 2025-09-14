using Gtk;
using System;
using System.IO;
using FanControl.Gui.Services;
using FanControl.Gui.Views;

namespace FanControl.Gui.Views;

public class DaemonStatusView : VBox
{
    private Label _statusLabel;
    private RpcClient _rpcClient;

    public DaemonStatusView()
    {
        Spacing = 10;
        _rpcClient = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        UpdateStatus();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void UpdateStatus()
    {
        string pidPath = "/run/pid/lfcd.pid";
        bool pidExists = File.Exists(pidPath);
        bool responds = _rpcClient.Ping();

        string statusKey = "daemon.status.unknown";

        if (pidExists && responds)
            statusKey = "daemon.status.running";
        else if (pidExists && !responds)
            statusKey = "daemon.status.unresponsive";
        else if (!pidExists && responds)
            statusKey = "daemon.status.nopid";
        else
            statusKey = "daemon.status.dead";

        _statusLabel.Text = Translation.Get(statusKey);
    }

    private void Redraw()
    {
        UpdateStatus();
    }
}
