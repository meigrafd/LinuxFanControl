using Gtk;
using FanControl.Gui.Views;
using FanControl.Gui.Widgets;

namespace FanControl.Gui.Views;

public class TriggerView : VBox
{
    private readonly TriggerEditor _editor;

    public TriggerView()
    {
        Spacing = 10;

        Label title = new(Translation.Get("trigger.title"));
        title.SetMarkup($"<b>{Translation.Get("trigger.title")}</b>");
        PackStart(title, false, false, 0);

        _editor = new TriggerEditor();
        PackStart(_editor, true, true, 0);
    }
}
