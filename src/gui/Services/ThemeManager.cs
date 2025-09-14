using Gtk;
using Gdk;
using System.IO;

namespace FanControl.Gui;

public static class ThemeManager
{
    private static readonly string ThemeDirectory = Path.Combine("themes");

    public static void Apply(string themeName)
    {
        string fileName = $"{themeName.ToLower()}.css";
        string path = Path.Combine(ThemeDirectory, fileName);

        if (!File.Exists(path))
            return;

        var cssProvider = new CssProvider();
        cssProvider.LoadFromPath(path);

        StyleContext.AddProviderForScreen(
            Screen.Default,
            cssProvider,
            StyleProviderPriority.User
        );
    }

    public static string[] AvailableThemes()
    {
        if (!Directory.Exists(ThemeDirectory))
            return new[] { "Light" };

        return Directory.GetFiles(ThemeDirectory, "*.css")
        .Select(f => Path.GetFileNameWithoutExtension(f))
        .ToArray();
    }
}
