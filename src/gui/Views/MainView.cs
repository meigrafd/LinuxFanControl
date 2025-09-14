using Gtk;
using FanControl.Gui.Views;

namespace FanControl.Gui.Views;

public class MainView : HBox
{
    private Sidebar _sidebar;
    private VBox _contentArea;

    private CurveView _curveView;
    private TriggerView _triggerView;
    private MixView _mixView;
    private SettingsView _settingsView;

    public MainView()
    {
        Spacing = 10;

        Translation.LanguageChanged += Redraw;

        _sidebar = new Sidebar();
        PackStart(_sidebar, false, false, 0);

        _contentArea = new VBox();
        PackStart(_contentArea, true, true, 0);

        _curveView = new CurveView();
        _triggerView = new TriggerView();
        _mixView = new MixView();
        _settingsView = new SettingsView();

        ShowView(_curveView); // Initiale View
    }

    private void ShowView(Widget view)
    {
        foreach (var child in _contentArea.Children)
            _contentArea.Remove(child);

        _contentArea.PackStart(view, true, true, 0);
        view.ShowAll();
    }

    private void Redraw()
    {
        _curveView = new CurveView();
        _triggerView = new TriggerView();
        _mixView = new MixView();
        _settingsView = new SettingsView();

        // Optional: aktuelle View neu anzeigen
        ShowView(_settingsView);
    }
}
