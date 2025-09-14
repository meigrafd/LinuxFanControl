using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Views;

public class HwmonDebugPanel : VBox
{
    private readonly RpcClient _rpc;

    public HwmonDebugPanel()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadRawData();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        var title = new Label();
        title.SetMarkup($"<b>{Translation.Get("hwmon.debug.title")}</b>");
        PackStart(title, false, false, 0);
    }

    private void LoadRawData()
    {
        var response = _rpc.SendRequest("enumerate");
        if (response is JsonObject obj && obj["result"] is JsonObject result)
        {
            if (result["sensors"] is JsonArray sensors)
            {
                PackStart(new Label(Translation.Get("hwmon.debug.sensors")), false, false, 0);
                foreach (var s in sensors)
                {
                    var line = new Label($"{s?["device"]}: {s?["label"]} ({s?["path"]})");
                    PackStart(line, false, false, 0);
                }
            }

            if (result["pwms"] is JsonArray pwms)
            {
                PackStart(new Label(Translation.Get("hwmon.debug.pwms")), false, false, 0);
                foreach (var p in pwms)
                {
                    var line = new Label($"{p?["device"]}: {p?["label"]} → {p?["pwm_path"]}");
                    PackStart(line, false, false, 0);
                }
            }
        }
    }

    private void Redraw()
    {
        BuildUi();
        LoadRawData();
    }
}
