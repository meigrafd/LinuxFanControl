using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlEventDispatcher : VBox
{
    private readonly RpcClient _rpc;

    private Entry _targetEntry;
    private ComboBoxText _formatSelector;
    private Button _registerButton;
    private Label _statusLabel;

    public FanControlEventDispatcher()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        _targetEntry = new Entry { PlaceholderText = Translation.Get("dispatch.target") };
        PackStart(_targetEntry, false, false, 0);

        _formatSelector = new ComboBoxText();
        _formatSelector.AppendText("JSON");
        _formatSelector.AppendText("Shell");
        _formatSelector.AppendText("MQTT");
        _formatSelector.Active = 0;
        PackStart(_formatSelector, false, false, 0);

        _registerButton = new Button(Translation.Get("dispatch.register"));
        _registerButton.Clicked += (_, _) => RegisterDispatcher();
        PackStart(_registerButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void RegisterDispatcher()
    {
        var target = _targetEntry.Text;
        var format = _formatSelector.ActiveText;

        if (!string.IsNullOrEmpty(target) && !string.IsNullOrEmpty(format))
        {
            var payload = new JsonObject
            {
                ["target"] = target,
                ["format"] = format
            };
            var response = _rpc.SendRequest("registerEventDispatcher", payload);
            if (response is JsonObject obj && obj["result"]?.ToString() == "ok")
            {
                _statusLabel.Text = Translation.Get("dispatch.success");
            }
            else
            {
                _statusLabel.Text = Translation.Get("dispatch.error");
            }
        }
        else
        {
            _statusLabel.Text = Translation.Get("dispatch.invalid");
        }
    }

    private void Redraw()
    {
        _registerButton.Label = Translation.Get("dispatch.register");
    }
}
