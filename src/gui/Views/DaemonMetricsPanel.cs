using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using GLib;

namespace FanControl.Gui.Views;

public class DaemonMetricsPanel : VBox
{
    private readonly RpcClient _rpc;
    private Label _cpuLabel;
    private Label _memLabel;

    public DaemonMetricsPanel()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        _cpuLabel = new Label();
        _memLabel = new Label();

        PackStart(new Label($"<b>{Translation.Get("metrics.title")}</b>") { UseMarkup = true }, false, false, 0);
        PackStart(_cpuLabel, false, false, 0);
        PackStart(_memLabel, false, false, 0);

        Translation.LanguageChanged += Redraw;

        UpdateMetrics();
        Timeout.Add(3000, () =>
        {
            UpdateMetrics();
            return true;
        });
    }

    private void UpdateMetrics()
    {
        var response = _rpc.SendRequest("getMetrics");
        if (response is JsonObject obj && obj["result"] is JsonObject metrics)
        {
            var cpu = metrics["cpu_usage"]?.ToString() ?? "?";
            var mem = metrics["memory_mb"]?.ToString() ?? "?";

            _cpuLabel.Text = $"{Translation.Get("metrics.cpu")}: {cpu}%";
            _memLabel.Text = $"{Translation.Get("metrics.memory")}: {mem} MB";
        }
        else
        {
            _cpuLabel.Text = Translation.Get("metrics.error");
            _memLabel.Text = "";
        }
    }

    private void Redraw()
    {
        UpdateMetrics();
    }
}
