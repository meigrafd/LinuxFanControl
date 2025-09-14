using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Views;

public class TriggerList : VBox
{
    private readonly RpcClient _rpc;

    public TriggerList()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadTriggers();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        var title = new Label();
        title.SetMarkup($"<b>{Translation.Get("trigger.list.title")}</b>");
        PackStart(title, false, false, 0);
    }

    private void LoadTriggers()
    {
        var response = _rpc.SendRequest("listTriggers");
        if (response is JsonObject obj && obj["result"] is JsonArray triggers)
        {
            foreach (var t in triggers)
            {
                var source = t?["source"]?.ToString() ?? "?";
                var direction = t?["direction"]?.ToString() ?? "?";
                var threshold = t?["threshold"]?.ToString() ?? "?";
                var target = t?["target"]?.ToString() ?? "?";

                var line = new Label($"{source} {direction} {threshold} → {target}");
                PackStart(line, false, false, 0);
            }
        }
    }

    private void Redraw()
    {
        BuildUi();
        LoadTriggers();
    }
}
