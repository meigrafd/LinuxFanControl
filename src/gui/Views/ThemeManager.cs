using Gtk;
using GLib;

namespace FanControl.Gui.Views;

public static class ThemeManager
{
    public static void Apply(string theme)
    {
        var settings = Gtk.Settings.Default;
        var preferDark = theme == "dark";
        settings.SetProperty("gtk-application-prefer-dark-theme", new Value(preferDark));
    }
}
