using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class SensorAliasManager : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sensorSelector;
    private Entry _aliasEntry;
    private Button _applyButton;
    private Label _statusLabel;

    public SensorAliasManager()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadSensors();
    }

    private void BuildUi()
    {
        _sensorSelector = new ComboBoxText();
        PackStart(_sensorSelector, false, false, 0);

        _aliasEntry = new Entry { PlaceholderText = Translation.Get("alias.name") };
        PackStart(_aliasEntry, false, false, 0);

        _applyButton = new Button(Translation.Get("alias.apply"));
        _applyButton.Clicked += (_, _) => ApplyAlias();
        PackStart(_applyButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadSensors()
    {
        _sensorSelector.RemoveAll();

        var response = _rpc.SendRequest("listSensors");
        if (response is JsonObject obj && obj["result"] is JsonArray sensors)
        {
            foreach (var s in sensors)
            {
                var label = s?["label"]?.ToString() ?? "?";
                var path = s?["path"]?.ToString() ?? "";
                _sensorSelector.AppendText($"{label} ({path})");
            }

            if (_sensorSelector.Children.Length > 0)
                _sensorSelector.Active = 0;
        }
    }

    private void ApplyAlias()
    {
        var sensor = _sensorSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        var alias = _aliasEntry.Text;

        if (!string.IsNullOrEmpty(sensor) && !string.IsNullOrEmpty(alias))
        {
            var payload = new JsonObject
            {
                ["path"] = sensor,
                ["alias"] = alias
            };
            var response = _rpc.SendRequest("setSensorAlias", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("alias.success")
            : Translation.Get("alias.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("alias.invalid");
        }
    }

    private void Redraw()
    {
        _applyButton.Label = Translation.Get("alias.apply");
    }
}
