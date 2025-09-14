using Gtk;
using FanControl.Gui.Services;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.IO;

namespace FanControl.Gui.Tools;

public class FanControlBackupTool : VBox
{
    private readonly RpcClient _rpc;
    private Button _backupButton;
    private Button _restoreButton;
    private Label _statusLabel;

    private const string BackupPath = "/etc/fancontrol/backup.json"; // Pfad ggf. anpassen

    public FanControlBackupTool()
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

        _backupButton = new Button(Translation.Get("backup.save"));
        _backupButton.Clicked += (_, _) => SaveBackup();
        PackStart(_backupButton, false, false, 0);

        _restoreButton = new Button(Translation.Get("backup.restore"));
        _restoreButton.Clicked += (_, _) => LoadBackup();
        PackStart(_restoreButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void SaveBackup()
    {
        var response = _rpc.SendRequest("listChannels");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            try
            {
                var json = JsonSerializer.Serialize(channels, new JsonSerializerOptions { WriteIndented = true });
                File.WriteAllText(BackupPath, json);
                _statusLabel.Text = Translation.Get("backup.success");
            }
            catch
            {
                _statusLabel.Text = Translation.Get("backup.error");
            }
        }
    }

    private void LoadBackup()
    {
        if (File.Exists(BackupPath))
        {
            try
            {
                var json = File.ReadAllText(BackupPath);
                var channels = JsonSerializer.Deserialize<JsonArray>(json);
                if (channels != null)
                {
                    foreach (var ch in channels)
                        _rpc.SendRequest("createChannel", ch.AsObject());

                    _statusLabel.Text = Translation.Get("backup.restored");
                }
                else
                {
                    _statusLabel.Text = Translation.Get("backup.invalid");
                }
            }
            catch
            {
                _statusLabel.Text = Translation.Get("backup.error");
            }
        }
        else
        {
            _statusLabel.Text = Translation.Get("backup.notfound");
        }
    }

    private void Redraw()
    {
        _backupButton.Label = Translation.Get("backup.save");
        _restoreButton.Label = Translation.Get("backup.restore");
    }
}
