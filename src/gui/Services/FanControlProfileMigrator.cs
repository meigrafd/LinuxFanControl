using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlProfileMigrator : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _profileSelector;
    private Entry _targetPathEntry;
    private Button _migrateButton;
    private Label _statusLabel;

    public FanControlProfileMigrator()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadProfiles();
    }

    private void BuildUi()
    {
        _profileSelector = new ComboBoxText();
        PackStart(_profileSelector, false, false, 0);

        _targetPathEntry = new Entry { PlaceholderText = Translation.Get("migrate.path") };
        PackStart(_targetPathEntry, false, false, 0);

        _migrateButton = new Button(Translation.Get("migrate.run"));
        _migrateButton.Clicked += (_, _) => MigrateProfile();
        PackStart(_migrateButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadProfiles()
    {
        _profileSelector.RemoveAll();

        var response = _rpc.SendRequest("listProfiles");
        if (response is JsonObject obj && obj["result"] is JsonArray profiles)
        {
            foreach (var p in profiles)
            {
                var name = p?.ToString() ?? "?";
                _profileSelector.AppendText(name);
            }

            if (_profileSelector.Children.Length > 0)
                _profileSelector.Active = 0;
        }
    }

    private void MigrateProfile()
    {
        var profile = _profileSelector.ActiveText;
        var path = _targetPathEntry.Text;

        if (!string.IsNullOrEmpty(profile) && !string.IsNullOrEmpty(path))
        {
            var payload = new JsonObject
            {
                ["name"] = profile,
                ["target"] = path
            };
            var response = _rpc.SendRequest("migrateFanProfile", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("migrate.success")
            : Translation.Get("migrate.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("migrate.invalid");
        }
    }

    private void Redraw()
    {
        _migrateButton.Label = Translation.Get("migrate.run");
    }
}
