using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class DaemonShutdownPlanner : VBox
{
    private readonly RpcClient _rpc;

    private Entry _delayEntry;
    private CheckButton _notifyCheck;
    private Button _shutdownButton;
    private Label _statusLabel;

    public DaemonShutdownPlanner()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        _delayEntry = new Entry { PlaceholderText = Translation.Get("shutdown.delay") };
        PackStart(_delayEntry, false, false, 0);

        _notifyCheck = new CheckButton(Translation.Get("shutdown.notify"));
        PackStart(_notifyCheck, false, false, 0);

        _shutdownButton = new Button(Translation.Get("shutdown.run"));
        _shutdownButton.Clicked += (_, _) => PlanShutdown();
        PackStart(_shutdownButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void PlanShutdown()
    {
        var delayText = _delayEntry.Text;
        var notify = _notifyCheck.Active;

        if (int.TryParse(delayText, out var delay))
        {
            var payload = new JsonObject
            {
                ["delay_seconds"] = delay,
                ["notify"] = notify
            };
            var response = _rpc.SendRequest("scheduleDaemonShutdown", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("shutdown.success")
            : Translation.Get("shutdown.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("shutdown.invalid");
        }
    }

    private void Redraw()
    {
        _shutdownButton.Label = Translation.Get("shutdown.run");
    }
}
