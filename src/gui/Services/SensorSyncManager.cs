using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class SensorSyncManager : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _primarySelector;
    private ComboBoxText _secondarySelector;
    private Button _syncButton;
    private Label _statusLabel;

    public SensorSyncManager()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadSensors();
    }

    private void BuildUi()
    {
        _primarySelector = new ComboBoxText();
        PackStart(_primarySelector, false, false, 0);

        _secondarySelector = new ComboBoxText();
        PackStart(_secondarySelector, false, false, 0);

        _syncButton = new Button(Translation.Get("sync.run"));
        _syncButton.Clicked += (_, _) => ApplySync();
        PackStart(_syncButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadSensors()
    {
        _primarySelector.RemoveAll();
        _secondarySelector.RemoveAll();

        var response = _rpc.SendRequest("listSensors");
        if (response is JsonObject obj && obj["result"] is JsonArray sensors)
        {
            foreach (var s in sensors)
            {
                var label = s?["label"]?.ToString() ?? "?";
                var path = s?["path"]?.ToString() ?? "";
                var entry = $"{label} ({path})";
                _primarySelector.AppendText(entry);
                _secondarySelector.AppendText(entry);
            }

            _primarySelector.Active = 0;
            _secondarySelector.Active = 0;
        }
    }

    private void ApplySync()
    {
        var primary = _primarySelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        var secondary = _secondarySelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');

        if (!string.IsNullOrEmpty(primary) && !string.IsNullOrEmpty(secondary) && primary != secondary)
        {
            var payload = new JsonObject
            {
                ["primary"] = primary,
                ["secondary"] = secondary
            };
            var response = _rpc.SendRequest("syncSensorPair", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("sync.success")
            : Translation.Get("sync.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("sync.invalid");
        }
    }

    private void Redraw()
    {
        _syncButton.Label = Translation.Get("sync.run");
    }
}
