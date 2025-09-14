using Gtk;
using FanControl.Gui.Views;
using FanControl.Gui.Widgets;

namespace FanControl.Gui.Views;

public class MixView : VBox
{
    private readonly MixEditor _editor;

    public MixView()
    {
        Spacing = 10;

        Label title = new(Translation.Get("mix.title"));
        title.SetMarkup($"<b>{Translation.Get("mix.title")}</b>");
        PackStart(title, false, false, 0);

        _editor = new MixEditor();
        PackStart(_editor, true, true, 0);
    }
}
