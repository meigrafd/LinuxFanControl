using Gtk;
using FanControl.Gui;
using System;

namespace FanControl.Gui.Views;

public class DaemonStatusView : Box
{
    private readonly Label _statusLabel;
    private readonly Button _refreshButton;

    public DaemonStatusView() : base(Orientation.Vertical, 10)
    {
        Margin = 20;

        _statusLabel = new Label("Status: unknown");
        _refreshButton = new Button("Refresh");

        _refreshButton.Clicked += (_, _) => UpdateStatus();

        PackStart(_statusLabel, false, false, 0);
        PackStart(_refreshButton, false, false, 0);

        UpdateStatus();
    }

    private void UpdateStatus()
    {
        bool running = CheckDaemonRunning();
        _statusLabel.Text = $"Status: {(running ? "active" : "inactive")}";
    }

    private bool CheckDaemonRunning()
    {
        try
        {
            var processes = System.Diagnostics.Process.GetProcessesByName("fancontrold");
            return processes.Length > 0;
        }
        catch
        {
            return false;
        }
    }
}
