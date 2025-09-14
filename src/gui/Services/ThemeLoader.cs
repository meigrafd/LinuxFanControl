using Gtk;
using System.IO;

namespace FanControl.Gui;

public static class ThemeLoader
{
    public static void Apply(string themeName)
    {
        string themePath = $"Themes/{themeName.ToLower()}.css";
        if (!File.Exists(themePath))
            return;

        var provider = new CssProvider();
        provider.LoadFromPath(themePath);

        StyleContext.AddProviderForScreen(
            Gdk.Screen.Default,
            provider,
            Gtk.StyleProviderPriority.User
        );
    }
}
