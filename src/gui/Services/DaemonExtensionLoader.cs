using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class DaemonExtensionLoader : VBox
{
    private readonly RpcClient _rpc;

    private Entry _pathEntry;
    private Button _loadButton;
    private Label _statusLabel;

    public DaemonExtensionLoader()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        _pathEntry = new Entry { PlaceholderText = Translation.Get("extension.path") };
        PackStart(_pathEntry, false, false, 0);

        _loadButton = new Button(Translation.Get("extension.load"));
        _loadButton.Clicked += (_, _) => LoadExtension();
        PackStart(_loadButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadExtension()
    {
        var path = _pathEntry.Text;
        if (!string.IsNullOrEmpty(path))
        {
            var payload = new JsonObject { ["path"] = path };
            var response = _rpc.SendRequest("loadDaemonExtension", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("extension.success")
            : Translation.Get("extension.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("extension.invalid");
        }
    }

    private void Redraw()
    {
        _loadButton.Label = Translation.Get("extension.load");
    }
}
