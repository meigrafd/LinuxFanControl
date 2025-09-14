using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Views;

public class CouplingView : VBox
{
    private readonly RpcClient _rpc;

    public CouplingView()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadCoupling();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        var title = new Label();
        title.SetMarkup($"<b>{Translation.Get("coupling.title")}</b>");
        PackStart(title, false, false, 0);
    }

    private void LoadCoupling()
    {
        var response = _rpc.SendRequest("detectCalibrate");
        if (response is JsonObject obj && obj["result"] is JsonObject result)
        {
            if (result["mapping"] is JsonArray map)
            {
                foreach (var entry in map)
                {
                    var pwm = entry?["pwm_label"]?.ToString() ?? "?";
                    var sensor = entry?["sensor_label"]?.ToString() ?? "?";
                    var path = entry?["sensor_path"]?.ToString() ?? "?";

                    var line = new Label($"{pwm} ← {sensor} ({path})");
                    PackStart(line, false, false, 0);
                }
            }
        }
    }

    private void Redraw()
    {
        BuildUi();
        LoadCoupling();
    }
}
