using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class DaemonAnalyticsReporter : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _reportTypeSelector;
    private Button _sendButton;
    private Label _statusLabel;

    public DaemonAnalyticsReporter()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        _reportTypeSelector = new ComboBoxText();
        _reportTypeSelector.AppendText("usage");
        _reportTypeSelector.AppendText("performance");
        _reportTypeSelector.AppendText("error-log");
        _reportTypeSelector.Active = 0;
        PackStart(_reportTypeSelector, false, false, 0);

        _sendButton = new Button(Translation.Get("analytics.send"));
        _sendButton.Clicked += (_, _) => SendReport();
        PackStart(_sendButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void SendReport()
    {
        var type = _reportTypeSelector.ActiveText;
        if (!string.IsNullOrEmpty(type))
        {
            var payload = new JsonObject { ["type"] = type };
            var response = _rpc.SendRequest("sendAnalyticsReport", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("analytics.success")
            : Translation.Get("analytics.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("analytics.invalid");
        }
    }

    private void Redraw()
    {
        _sendButton.Label = Translation.Get("analytics.send");
    }
}
