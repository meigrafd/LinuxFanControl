using Gtk;
using FanControl.Gui.Views;

namespace FanControl.Gui.Views;

public class CurveView : VBox
{
    private Label _titleLabel;
    private Button _saveButton;
    private Button _resetButton;
    private Button _clearButton;

    public CurveView()
    {
        Spacing = 10;

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _titleLabel = new Label(Translation.Get("curve.title"));
        _titleLabel.SetMarkup($"<b>{Translation.Get("curve.title")}</b>");
        PackStart(_titleLabel, false, false, 0);

        _saveButton = new Button(Translation.Get("curve.save"));
        PackStart(_saveButton, false, false, 0);

        _resetButton = new Button(Translation.Get("curve.reset"));
        PackStart(_resetButton, false, false, 0);

        _clearButton = new Button(Translation.Get("curve.clear"));
        PackStart(_clearButton, false, false, 0);

        // Hier kannst du später weitere Widgets wie CurveEditorContainer hinzufügen
    }

    private void Redraw()
    {
        BuildUi();
    }
}
