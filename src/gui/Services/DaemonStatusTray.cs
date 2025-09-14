using Gtk;
using FanControl.Gui.Services;

namespace FanControl.Gui.System;

public class DaemonStatusTray
{
    private readonly RpcClient _rpc;
    private StatusIcon _trayIcon;
    private Menu _trayMenu;
    private MenuItem _statusItem;
    private MenuItem _quitItem;

    public DaemonStatusTray()
    {
        _rpc = new RpcClient();

        _trayIcon = new StatusIcon
        {
            Pixbuf = Pixbuf.LoadFromResource("fancontrol-icon.png"),
            Tooltip = "FanControl Daemon",
            Visible = true
        };

        _trayMenu = new Menu();

        _statusItem = new MenuItem("Status: ...");
        _trayMenu.Append(_statusItem);

        _quitItem = new MenuItem("Quit");
        _quitItem.Activated += (_, _) => Application.Quit();
        _trayMenu.Append(_quitItem);

        _trayMenu.ShowAll();
        _trayIcon.PopupMenu += (_, args) => _trayMenu.Popup();

        GLib.Timeout.Add(5000, () =>
        {
            UpdateStatus();
            return true;
        });

        UpdateStatus();
    }

    private void UpdateStatus()
    {
        bool pidExists = System.IO.File.Exists("/run/pid/lfcd.pid");
        bool responds = _rpc.Ping();

        string status = "Unknown";
        if (pidExists && responds)
            status = "Running";
        else if (pidExists && !responds)
            status = "Unresponsive";
        else if (!pidExists && responds)
            status = "No PID";
        else
            status = "Stopped";

        _statusItem.Label = $"Status: {status}";
    }
}
