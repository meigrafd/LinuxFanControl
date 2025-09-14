using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using GLib;

namespace FanControl.Gui.Views;

public class DaemonInterfaceMonitor : VBox
{
    private readonly RpcClient _rpc;
    private Label _gpioLabel;
    private Label _i2cLabel;
    private Label _netLabel;

    public DaemonInterfaceMonitor()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        _gpioLabel = new Label();
        _i2cLabel = new Label();
        _netLabel = new Label();

        PackStart(new Label($"<b>{Translation.Get("interface.title")}</b>") { UseMarkup = true }, false, false, 0);
        PackStart(_gpioLabel, false, false, 0);
        PackStart(_i2cLabel, false, false, 0);
        PackStart(_netLabel, false, false, 0);

        Translation.LanguageChanged += Redraw;

        UpdateStatus();
        Timeout.Add(5000, () =>
        {
            UpdateStatus();
            return true;
        });
    }

    private void UpdateStatus()
    {
        var response = _rpc.SendRequest("getInterfaceStatus");
        if (response is JsonObject obj && obj["result"] is JsonObject status)
        {
            _gpioLabel.Text = $"{Translation.Get("interface.gpio")}: {status["gpio"]}";
            _i2cLabel.Text = $"{Translation.Get("interface.i2c")}: {status["i2c"]}";
            _netLabel.Text = $"{Translation.Get("interface.network")}: {status["network"]}";
        }
        else
        {
            _gpioLabel.Text = Translation.Get("interface.error");
            _i2cLabel.Text = "";
            _netLabel.Text = "";
        }
    }

    private void Redraw()
    {
        UpdateStatus();
    }
}
