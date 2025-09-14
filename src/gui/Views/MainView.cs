using Gtk;
using FanControl.Gui.Views;
using FanControl.Gui.Widgets;

namespace FanControl.Gui.Views;

public class MainView : HBox
{
    private readonly Sidebar _sidebar;
    private readonly Stack _contentStack;

    public MainView()
    {
        _sidebar = new Sidebar();
        _sidebar.SectionSelected += OnSectionSelected;
        PackStart(_sidebar, false, false, 0);

        _contentStack = new Stack();
        _contentStack.AddNamed(new SensorsView(), "sensors");
        _contentStack.AddNamed(new CurveView(), "curve");
        _contentStack.AddNamed(new TriggerView(), "trigger");
        _contentStack.AddNamed(new MixView(), "mix");
        _contentStack.AddNamed(new SettingsView(), "settings");

        PackStart(_contentStack, true, true, 0);

        _contentStack.VisibleChildName = "sensors";
    }

    private void OnSectionSelected(string sectionId)
    {
        _contentStack.VisibleChildName = sectionId;
    }
}
