using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;
using FanControl.Gui.Widgets;

namespace FanControl.Gui.Views;

public class FanGroupView : VBox
{
    private readonly RpcClient _rpc;

    public FanGroupView()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadFans();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        var title = new Label();
        title.SetMarkup($"<b>{Translation.Get("fan.group.title")}</b>");
        PackStart(title, false, false, 0);
    }

    private void LoadFans()
    {
        var response = _rpc.SendRequest("listPwms");
        if (response is JsonObject obj && obj["result"] is JsonArray fans)
        {
            foreach (var fan in fans)
            {
                var label = fan?["label"]?.ToString() ?? "Unnamed";
                var tile = new FanTile(label);
                PackStart(tile, false, false, 0);
            }
        }
    }

    private void Redraw()
    {
        BuildUi();
        LoadFans();
    }
}
