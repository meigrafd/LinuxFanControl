using Gtk;
using FanControl.Gui.Views;

namespace FanControl.Gui.Views;

public class Sidebar : VBox
{
    private Button _sensorsButton;
    private Button _curveButton;
    private Button _triggerButton;
    private Button _mixButton;
    private Button _settingsButton;

    public Sidebar()
    {
        Spacing = 5;

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _sensorsButton = new Button(Translation.Get("sidebar.sensors"));
        PackStart(_sensorsButton, false, false, 0);

        _curveButton = new Button(Translation.Get("sidebar.curve"));
        PackStart(_curveButton, false, false, 0);

        _triggerButton = new Button(Translation.Get("sidebar.trigger"));
        PackStart(_triggerButton, false, false, 0);

        _mixButton = new Button(Translation.Get("sidebar.mix"));
        PackStart(_mixButton, false, false, 0);

        _settingsButton = new Button(Translation.Get("sidebar.settings"));
        PackStart(_settingsButton, false, false, 0);

        // Hier kannst du Click-Events oder Navigation hinzufügen
    }

    private void Redraw()
    {
        BuildUi();
    }
}
