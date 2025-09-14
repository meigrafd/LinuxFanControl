using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlCloudBridge : VBox
{
    private readonly RpcClient _rpc;

    private Entry _tokenEntry;
    private Button _connectButton;
    private Label _statusLabel;

    public FanControlCloudBridge()
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

        _tokenEntry = new Entry { PlaceholderText = Translation.Get("cloud.token") };
        PackStart(_tokenEntry, false, false, 0);

        _connectButton = new Button(Translation.Get("cloud.connect"));
        _connectButton.Clicked += (_, _) => ConnectCloud();
        PackStart(_connectButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void ConnectCloud()
    {
        var token = _tokenEntry.Text;
        if (!string.IsNullOrEmpty(token))
        {
            var payload = new JsonObject { ["token"] = token };
            var response = _rpc.SendRequest("connectCloudBridge", payload);
            if (response is JsonObject obj && obj["result"]?.ToString() == "ok")
            {
                _statusLabel.Text = Translation.Get("cloud.success");
            }
            else
            {
                _statusLabel.Text = Translation.Get("cloud.error");
            }
        }
        else
        {
            _statusLabel.Text = Translation.Get("cloud.invalid");
        }
    }

    private void Redraw()
    {
        _connectButton.Label = Translation.Get("cloud.connect");
    }
}
