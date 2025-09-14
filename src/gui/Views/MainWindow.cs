using Gtk;
using FanControl.Gui.Views;
using FanControl.Gui.Widgets;
using System.Collections.Generic;

namespace FanControl.Gui;

public class MainWindow : Window
{
    public MainWindow() : base(Translation.T("title"))
    {
        SetDefaultSize(800, 600);
        SetPosition(WindowPosition.Center);

        var stack = new Stack
        {
            TransitionType = StackTransitionType.Crossfade
        };

        var views = new Dictionary<string, Widget>
        {
            { Translation.T("sidebar.sensors"), new SensorTile() },
            { Translation.T("sidebar.curve"), new CurveView() },
            { Translation.T("sidebar.settings"), new SettingsView() }
        };

        foreach (var view in views)
            stack.AddNamed(view.Value, view.Key);

        var sidebar = new Sidebar(views, stack);

        var layout = new Box(Orientation.Horizontal, 0);
        layout.PackStart(sidebar, false, false, 0);
        layout.PackStart(stack, true, true, 0);

        Add(layout);
        ShowAll();
    }
}
