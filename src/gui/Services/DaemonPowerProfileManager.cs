using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class DaemonPowerProfileManager : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _profileSelector;
    private Button _applyButton;
    private Label _statusLabel;

    public DaemonPowerProfileManager()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadPowerProfiles();
    }

    private void BuildUi()
    {
        _profileSelector = new ComboBoxText();
        PackStart(_profileSelector, false, false, 0);

        _applyButton = new Button(Translation.Get("power.apply"));
        _applyButton.Clicked += (_, _) => ApplyPowerProfile();
        PackStart(_applyButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadPowerProfiles()
    {
        _profileSelector.RemoveAll();

        var response = _rpc.SendRequest("listPowerProfiles");
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

    private void ApplyPowerProfile()
    {
        var profile = _profileSelector.ActiveText;
        if (!string.IsNullOrEmpty(profile))
        {
            var payload = new JsonObject { ["name"] = profile };
            var response = _rpc.SendRequest("applyPowerProfile", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("power.success")
            : Translation.Get("power.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("power.invalid");
        }
    }

    private void Redraw()
    {
        _applyButton.Label = Translation.Get("power.apply");
    }
}
