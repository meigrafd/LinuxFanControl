using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Views;

public class SensorAlertPanel : VBox
{
    private readonly RpcClient _rpc;

    public SensorAlertPanel()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadAlerts();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        var title = new Label();
        title.SetMarkup($"<b>{Translation.Get("alerts.title")}</b>");
        PackStart(title, false, false, 0);
    }

    private void LoadAlerts()
    {
        var response = _rpc.SendRequest("listSensors");
        if (response is JsonObject obj && obj["result"] is JsonArray sensors)
        {
            foreach (var sensor in sensors)
            {
                var label = sensor?["label"]?.ToString() ?? "?";
                var value = sensor?["value"]?.GetValue<double>() ?? 0.0;
                var unit = sensor?["unit"]?.ToString() ?? "°C";

                if (value > 80.0) // Schwellenwert konfigurierbar machen?
                {
                    var alert = new Label($"{label}: {value:F1}{unit} ⚠️");
                    PackStart(alert, false, false, 0);
                }
            }
        }
    }

    private void Redraw()
    {
        BuildUi();
        LoadAlerts();
    }
}
