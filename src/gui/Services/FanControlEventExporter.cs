using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlEventExporter : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _formatSelector;
    private Entry _filenameEntry;
    private Button _exportButton;
    private Label _statusLabel;

    public FanControlEventExporter()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        _formatSelector = new ComboBoxText();
        _formatSelector.AppendText("json");
        _formatSelector.AppendText("csv");
        _formatSelector.AppendText("xml");
        _formatSelector.Active = 0;
        PackStart(_formatSelector, false, false, 0);

        _filenameEntry = new Entry { PlaceholderText = Translation.Get("event.filename") };
        PackStart(_filenameEntry, false, false, 0);

        _exportButton = new Button(Translation.Get("event.export"));
        _exportButton.Clicked += (_, _) => ExportEvents();
        PackStart(_exportButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void ExportEvents()
    {
        var format = _formatSelector.ActiveText;
        var filename = _filenameEntry.Text;

        if (!string.IsNullOrEmpty(format) && !string.IsNullOrEmpty(filename))
        {
            var payload = new JsonObject
            {
                ["format"] = format,
                ["filename"] = filename
            };
            var response = _rpc.SendRequest("exportFanEvents", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("event.success")
            : Translation.Get("event.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("event.invalid");
        }
    }

    private void Redraw()
    {
        _exportButton.Label = Translation.Get("event.export");
    }
}
