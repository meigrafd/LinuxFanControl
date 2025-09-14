using Gtk;
using FanControl.Gui.Views;

namespace FanControl.Gui.Views;

public class MixView : VBox
{
    private Label _titleLabel;
    private Button _addButton;
    private Button _removeButton;
    private Label _methodLabel;
    private Label _outputLabel;

    public MixView()
    {
        Spacing = 10;

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _titleLabel = new Label();
        _titleLabel.SetMarkup($"<b>{Translation.Get("mix.title")}</b>");
        PackStart(_titleLabel, false, false, 0);

        _addButton = new Button(Translation.Get("mix.add"));
        PackStart(_addButton, false, false, 0);

        _removeButton = new Button(Translation.Get("mix.remove"));
        PackStart(_removeButton, false, false, 0);

        _methodLabel = new Label($"{Translation.Get("mix.method.max")} / {Translation.Get("mix.method.min")} / {Translation.Get("mix.method.avg")}");
        PackStart(_methodLabel, false, false, 0);

        _outputLabel = new Label(Translation.Get("mix.output"));
        PackStart(_outputLabel, false, false, 0);

        // Hier kannst du später MixEditor oder MixRow hinzufügen
    }

    private void Redraw()
    {
        BuildUi();
    }
}
