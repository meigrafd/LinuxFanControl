using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using GLib;

namespace FanControl.Gui.Views;

public class DaemonPowerStats : VBox
{
    private readonly RpcClient _rpc;
    private Label _voltageLabel;
    private Label _currentLabel;
    private Label _powerLabel;

    public DaemonPowerStats()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        _voltageLabel = new Label();
        _currentLabel = new Label();
        _powerLabel = new Label();

        PackStart(new Label($"<b>{Translation.Get("power.title")}</b>") { UseMarkup = true }, false, false, 0);
        PackStart(_voltageLabel, false, false, 0);
        PackStart(_currentLabel, false, false, 0);
        PackStart(_powerLabel, false, false, 0);

        Translation.LanguageChanged += Redraw;

        UpdateStats();
        Timeout.Add(5000, () =>
        {
            UpdateStats();
            return true;
        });
    }

    private void UpdateStats()
    {
        var response = _rpc.SendRequest("getPowerStats");
        if (response is JsonObject obj && obj["result"] is JsonObject stats)
        {
            _voltageLabel.Text = $"{Translation.Get("power.voltage")}: {stats["voltage"]} V";
            _currentLabel.Text = $"{Translation.Get("power.current")}: {stats["current"]} A";
            _powerLabel.Text = $"{Translation.Get("power.total")}: {stats["power"]} W";
        }
        else
        {
            _voltageLabel.Text = Translation.Get("power.error");
            _currentLabel.Text = "";
            _powerLabel.Text = "";
        }
    }

    private void Redraw()
    {
        UpdateStats();
    }
}
