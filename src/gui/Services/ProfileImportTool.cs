using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Text.Json;
using System.IO;

namespace FanControl.Gui.Tools;

public class ProfileImportTool : VBox
{
    private readonly RpcClient _rpc;
    private Entry _pathEntry;
    private Button _importButton;
    private Label _statusLabel;

    public ProfileImportTool()
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

        _pathEntry = new Entry { PlaceholderText = Translation.Get("import.path") };
        PackStart(_pathEntry, false, false, 0);

        _importButton = new Button(Translation.Get("import.load"));
        _importButton.Clicked += (_, _) => ImportProfiles();
        PackStart(_importButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void ImportProfiles()
    {
        var path = _pathEntry.Text;
        if (File.Exists(path))
        {
            try
            {
                var json = File.ReadAllText(path);
                var channels = JsonSerializer.Deserialize<JsonArray>(json);
                if (channels != null)
                {
                    foreach (var ch in channels)
                    {
                        _rpc.SendRequest("createChannel", ch.AsObject());
                    }
                    _statusLabel.Text = Translation.Get("import.success");
                }
                else
                {
                    _statusLabel.Text = Translation.Get("import.invalid");
                }
            }
            catch
            {
                _statusLabel.Text = Translation.Get("import.error");
            }
        }
        else
        {
            _statusLabel.Text = Translation.Get("import.notfound");
        }
    }

    private void Redraw()
    {
        _importButton.Label = Translation.Get("import.load");
    }
}
