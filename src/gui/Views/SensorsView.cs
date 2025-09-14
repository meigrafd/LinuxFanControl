using Gtk;
using FanControl.Gui.Views;

namespace FanControl.Gui.Views;

public class SensorsView : VBox
{
    private Label _titleLabel;

    public SensorsView()
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
        _titleLabel.SetMarkup($"<b>{Translation.Get("sidebar.sensors")}</b>");
        PackStart(_titleLabel, false, false, 0);

        // Hier kannst du später SensorTile-Widgets hinzufügen
    }

    private void Redraw()
    {
        BuildUi();
    }
}
