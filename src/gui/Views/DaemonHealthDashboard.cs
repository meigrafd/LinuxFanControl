using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using GLib;

namespace FanControl.Gui.Views;

public class DaemonHealthDashboard : VBox
{
    private readonly RpcClient _rpc;
    private Label _statusLabel;
    private Label _warningsLabel;
    private Label _uptimeLabel;

    public DaemonHealthDashboard()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        _statusLabel = new Label();
        _warningsLabel = new Label();
        _uptimeLabel = new Label();

        PackStart(new Label($"<b>{Translation.Get("health.title")}</b>") { UseMarkup = true }, false, false, 0);
        PackStart(_statusLabel, false, false, 0);
        PackStart(_warningsLabel, false, false, 0);
        PackStart(_uptimeLabel, false, false, 0);

        Translation.LanguageChanged += Redraw;

        UpdateHealth();
        Timeout.Add(5000, () =>
        {
            UpdateHealth();
            return true;
        });
    }

    private void UpdateHealth()
    {
        var response = _rpc.SendRequest("getDaemonHealth");
        if (response is JsonObject obj && obj["result"] is JsonObject health)
        {
            _statusLabel.Text = $"{Translation.Get("health.status")}: {health["status"]}";
            _warningsLabel.Text = $"{Translation.Get("health.warnings")}: {health["warnings"]}";
            _uptimeLabel.Text = $"{Translation.Get("health.uptime")}: {health["uptime"]}";
        }
        else
        {
            _statusLabel.Text = Translation.Get("health.error");
            _warningsLabel.Text = "";
            _uptimeLabel.Text = "";
        }
    }

    private void Redraw()
    {
        UpdateHealth();
    }
}
