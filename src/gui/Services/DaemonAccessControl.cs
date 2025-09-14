using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class DaemonAccessControl : VBox
{
    private readonly RpcClient _rpc;

    private Entry _userEntry;
    private ComboBoxText _roleSelector;
    private Button _grantButton;
    private Label _statusLabel;

    public DaemonAccessControl()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _userEntry = new Entry { PlaceholderText = Translation.Get("access.user") };
        PackStart(_userEntry, false, false, 0);

        _roleSelector = new ComboBoxText();
        _roleSelector.AppendText("viewer");
        _roleSelector.AppendText("editor");
        _roleSelector.AppendText("admin");
        _roleSelector.Active = 0;
        PackStart(_roleSelector, false, false, 0);

        _grantButton = new Button(Translation.Get("access.grant"));
        _grantButton.Clicked += (_, _) => GrantAccess();
        PackStart(_grantButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void GrantAccess()
    {
        var user = _userEntry.Text;
        var role = _roleSelector.ActiveText;

        if (!string.IsNullOrEmpty(user) && !string.IsNullOrEmpty(role))
        {
            var payload = new JsonObject
            {
                ["user"] = user,
                ["role"] = role
            };
            _rpc.SendRequest("setAccessRole", payload);
            _statusLabel.Text = Translation.Get("access.success");
        }
        else
        {
            _statusLabel.Text = Translation.Get("access.invalid");
        }
    }

    private void Redraw()
    {
        _grantButton.Label = Translation.Get("access.grant");
    }
}
