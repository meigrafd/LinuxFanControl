using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlRecoveryTool : VBox
{
    private readonly RpcClient _rpc;

    private Button _validateButton;
    private Button _rollbackButton;
    private Label _statusLabel;

    public FanControlRecoveryTool()
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

        _validateButton = new Button(Translation.Get("recovery.validate"));
        _validateButton.Clicked += (_, _) => ValidateConfig();
        PackStart(_validateButton, false, false, 0);

        _rollbackButton = new Button(Translation.Get("recovery.rollback"));
        _rollbackButton.Clicked += (_, _) => RollbackConfig();
        PackStart(_rollbackButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void ValidateConfig()
    {
        var response = _rpc.SendRequest("validateConfig");
        if (response is JsonObject obj && obj["result"]?.ToString() == "ok")
        {
            _statusLabel.Text = Translation.Get("recovery.valid");
        }
        else
        {
            _statusLabel.Text = Translation.Get("recovery.invalid");
        }
    }

    private void RollbackConfig()
    {
        var response = _rpc.SendRequest("rollbackConfig");
        if (response is JsonObject obj && obj["result"]?.ToString() == "ok")
        {
            _statusLabel.Text = Translation.Get("recovery.restored");
        }
        else
        {
            _statusLabel.Text = Translation.Get("recovery.error");
        }
    }

    private void Redraw()
    {
        _validateButton.Label = Translation.Get("recovery.validate");
        _rollbackButton.Label = Translation.Get("recovery.rollback");
    }
}
