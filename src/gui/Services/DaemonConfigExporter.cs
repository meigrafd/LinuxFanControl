using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class DaemonConfigExporter : VBox
{
    private readonly RpcClient _rpc;

    private Entry _filenameEntry;
    private Button _exportButton;
    private Label _statusLabel;

    public DaemonConfigExporter()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        _filenameEntry = new Entry { PlaceholderText = Translation.Get("export.filename") };
        PackStart(_filenameEntry, false, false, 0);

        _exportButton = new Button(Translation.Get("export.run"));
        _exportButton.Clicked += (_, _) => ExportConfig();
        PackStart(_exportButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void ExportConfig()
    {
        var name = _filenameEntry.Text;
        if (!string.IsNullOrEmpty(name))
        {
            var payload = new JsonObject { ["filename"] = name };
            var response = _rpc.SendRequest("exportConfiguration", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("export.success")
            : Translation.Get("export.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("export.invalid");
        }
    }

    private void Redraw()
    {
        _exportButton.Label = Translation.Get("export.run");
    }
}
