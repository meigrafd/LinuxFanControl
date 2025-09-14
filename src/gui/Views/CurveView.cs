using Gtk;
using FanControl.Gui.Views;
using FanControl.Gui.Widgets;

namespace FanControl.Gui.Views;

public class CurveView : VBox
{
    private readonly CurveEditorContainer _editor;

    public CurveView()
    {
        Spacing = 10;

        Label title = new(Translation.Get("curve.title"));
        title.SetMarkup($"<b>{Translation.Get("curve.title")}</b>");
        PackStart(title, false, false, 0);

        _editor = new CurveEditorContainer();
        PackStart(_editor, true, true, 0);
    }
}
