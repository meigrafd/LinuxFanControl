using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class SensorTopologyExporter : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _formatSelector;
    private Entry _filenameEntry;
    private Button _exportButton;
    private Label _statusLabel;

    public SensorTopologyExporter()
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
        _formatSelector.AppendText("graphviz");
        _formatSelector.Active = 0;
        PackStart(_formatSelector, false, false, 0);

        _filenameEntry = new Entry { PlaceholderText = Translation.Get("topology.filename") };
        PackStart(_filenameEntry, false, false, 0);

        _exportButton = new Button(Translation.Get("topology.export"));
        _exportButton.Clicked += (_, _) => ExportTopology();
        PackStart(_exportButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void ExportTopology()
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
            var response = _rpc.SendRequest("exportSensorTopology", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("topology.success")
            : Translation.Get("topology.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("topology.invalid");
        }
    }

    private void Redraw()
    {
        _exportButton.Label = Translation.Get("topology.export");
    }
}
