using Gtk;
using FanControl.Gui.Services;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.IO;

namespace FanControl.Gui.Tools;

public class ChannelExportTool : VBox
{
    private readonly RpcClient _rpc;
    private Button _exportButton;
    private Label _statusLabel;

    public ChannelExportTool()
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

        _exportButton = new Button(Translation.Get("export.channels"));
        _exportButton.Clicked += (_, _) => ExportChannels();
        PackStart(_exportButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void ExportChannels()
    {
        var response = _rpc.SendRequest("listChannels");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            var json = JsonSerializer.Serialize(channels, new JsonSerializerOptions { WriteIndented = true });
            var path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Desktop), "fancontrol-channels.json");

            try
            {
                File.WriteAllText(path, json);
                _statusLabel.Text = Translation.Get("export.success") + $"\n→ {path}";
            }
            catch
            {
                _statusLabel.Text = Translation.Get("export.error");
            }
        }
    }

    private void Redraw()
    {
        _exportButton.Label = Translation.Get("export.channels");
    }
}
