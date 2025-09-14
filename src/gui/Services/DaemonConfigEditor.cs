using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class DaemonConfigEditor : VBox
{
    private readonly RpcClient _rpc;

    private Entry _intervalEntry;
    private CheckButton _loggingToggle;
    private Button _applyButton;
    private Label _statusLabel;

    public DaemonConfigEditor()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadConfig();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _intervalEntry = new Entry { PlaceholderText = Translation.Get("config.interval") };
        PackStart(_intervalEntry, false, false, 0);

        _loggingToggle = new CheckButton(Translation.Get("config.logging"));
        PackStart(_loggingToggle, false, false, 0);

        _applyButton = new Button(Translation.Get("config.apply"));
        _applyButton.Clicked += (_, _) => ApplyConfig();
        PackStart(_applyButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadConfig()
    {
        var response = _rpc.SendRequest("getConfig");
        if (response is JsonObject obj && obj["result"] is JsonObject cfg)
        {
            _intervalEntry.Text = cfg["interval_ms"]?.ToString() ?? "1000";
            _loggingToggle.Active = cfg["logging_enabled"]?.GetValue<bool>() ?? false;
        }
    }

    private void ApplyConfig()
    {
        if (int.TryParse(_intervalEntry.Text, out var interval))
        {
            var payload = new JsonObject
            {
                ["interval_ms"] = interval,
                ["logging_enabled"] = _loggingToggle.Active
            };
            _rpc.SendRequest("setConfig", payload);
            _statusLabel.Text = Translation.Get("config.success");
        }
        else
        {
            _statusLabel.Text = Translation.Get("config.invalid");
        }
    }

    private void Redraw()
    {
        _applyButton.Label = Translation.Get("config.apply");
        _loggingToggle.Label = Translation.Get("config.logging");
    }
}
