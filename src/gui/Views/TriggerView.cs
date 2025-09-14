using Gtk;
using FanControl.Gui.Views;

namespace FanControl.Gui.Views;

public class TriggerView : VBox
{
    private Label _titleLabel;
    private Button _addButton;
    private Button _removeButton;
    private Label _directionLabel;
    private Label _targetLabel;

    public TriggerView()
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
        _titleLabel.SetMarkup($"<b>{Translation.Get("trigger.title")}</b>");
        PackStart(_titleLabel, false, false, 0);

        _addButton = new Button(Translation.Get("trigger.add"));
        PackStart(_addButton, false, false, 0);

        _removeButton = new Button(Translation.Get("trigger.remove"));
        PackStart(_removeButton, false, false, 0);

        _directionLabel = new Label($"{Translation.Get("trigger.direction.greater")} / {Translation.Get("trigger.direction.less")}");
        PackStart(_directionLabel, false, false, 0);

        _targetLabel = new Label(Translation.Get("trigger.target"));
        PackStart(_targetLabel, false, false, 0);

        // Hier kannst du später TriggerEditor oder TriggerRow hinzufügen
    }

    private void Redraw()
    {
        BuildUi();
    }
}
