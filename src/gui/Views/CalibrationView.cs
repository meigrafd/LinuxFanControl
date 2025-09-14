using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Views;

public class CalibrationView : VBox
{
    private readonly RpcClient _rpc;

    public CalibrationView()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadCalibration();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        var title = new Label();
        title.SetMarkup($"<b>{Translation.Get("calibration.title")}</b>");
        PackStart(title, false, false, 0);
    }

    private void LoadCalibration()
    {
        var response = _rpc.SendRequest("detectCalibrate");
        if (response is JsonObject obj && obj["result"] is JsonObject result)
        {
            if (result["calibration"] is JsonObject cal)
            {
                foreach (var kvp in cal.AsObject())
                {
                    var label = kvp.Key;
                    var data = kvp.Value?.AsObject();

                    var ok = data?["ok"]?.ToString() ?? "false";
                    var min = data?["min_pct"]?.ToString() ?? "?";
                    var spinup = data?["spinup_pct"]?.ToString() ?? "?";
                    var rpm = data?["rpm_at_min"]?.ToString() ?? "?";
                    var error = data?["error"]?.ToString() ?? "";

                    var line = new Label($"{label}: ok={ok}, min={min}%, spinup={spinup}%, rpm={rpm} {error}");
                    PackStart(line, false, false, 0);
                }
            }
        }
    }

    private void Redraw()
    {
        BuildUi();
        LoadCalibration();
    }
}
