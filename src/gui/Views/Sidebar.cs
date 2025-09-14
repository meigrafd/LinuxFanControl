using Gtk;
using System.Collections.Generic;

namespace FanControl.Gui.Views;

public class Sidebar : Box
{
    public Sidebar(Dictionary<string, Widget> views, Stack stack) : base(Orientation.Vertical, 0)
    {
        WidthRequest = 150;
        Margin = 0;
        Spacing = 0;

        foreach (var entry in views)
        {
            var button = new Button(entry.Key)
            {
                Halign = Align.Fill
            };

            button.Clicked += (_, _) =>
            {
                stack.VisibleChild = entry.Value;
            };

            PackStart(button, false, false, 0);
        }
    }
}
