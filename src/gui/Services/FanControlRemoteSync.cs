using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlRemoteSync : VBox
{
    private readonly RpcClient _rpc;

    private Entry _remoteUrlEntry;
    private Button _syncButton;
    private Label _statusLabel;

    public FanControlRemoteSync()
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

        _remoteUrlEntry = new Entry { PlaceholderText = Translation.Get("remote.url") };
        PackStart(_remoteUrlEntry, false, false, 0);

        _syncButton = new Button(Translation.Get("remote.sync"));
        _syncButton.Clicked += (_, _) => SyncRemote();
        PackStart(_syncButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void SyncRemote()
    {
        var url = _remoteUrlEntry.Text;
        if (!string.IsNullOrEmpty(url))
        {
            var payload = new JsonObject { ["url"] = url };
            var response = _rpc.SendRequest("syncFromRemote", payload);
            if (response is JsonObject obj && obj["result"]?.ToString() == "ok")
            {
                _statusLabel.Text = Translation.Get("remote.success");
            }
            else
            {
                _statusLabel.Text = Translation.Get("remote.error");
            }
        }
        else
        {
            _statusLabel.Text = Translation.Get("remote.invalid");
        }
    }

    private void Redraw()
    {
        _syncButton.Label = Translation.Get("remote.sync");
    }
}
