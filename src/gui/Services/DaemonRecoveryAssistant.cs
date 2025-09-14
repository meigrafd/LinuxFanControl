using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class DaemonRecoveryAssistant : VBox
{
    private readonly RpcClient _rpc;

    private Button _scanButton;
    private Button _restoreButton;
    private Label _statusLabel;

    public DaemonRecoveryAssistant()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        _scanButton = new Button(Translation.Get("recovery.scan"));
        _scanButton.Clicked += (_, _) => ScanRecoveryPoints();
        PackStart(_scanButton, false, false, 0);

        _restoreButton = new Button(Translation.Get("recovery.restore"));
        _restoreButton.Clicked += (_, _) => RestoreLastPoint();
        PackStart(_restoreButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void ScanRecoveryPoints()
    {
        var response = _rpc.SendRequest("scanRecoveryPoints");
        if (response is JsonObject obj && obj["result"] is JsonArray points)
        {
            _statusLabel.Text = points.Count > 0
            ? Translation.Get("recovery.found") + $": {points.Count}"
            : Translation.Get("recovery.none");
        }
        else
        {
            _statusLabel.Text = Translation.Get("recovery.error");
        }
    }

    private void RestoreLastPoint()
    {
        var response = _rpc.SendRequest("restoreLastRecoveryPoint");
        _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
        ? Translation.Get("recovery.success")
        : Translation.Get("recovery.error");
    }

    private void Redraw()
    {
        _scanButton.Label = Translation.Get("recovery.scan");
        _restoreButton.Label = Translation.Get("recovery.restore");
    }
}
