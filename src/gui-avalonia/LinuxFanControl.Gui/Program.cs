using Avalonia;
using System;

namespace LinuxFanControl.Gui;
class Program
{
    public static void Main(string[] args) => BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    public static AppBuilder BuildAvaloniaApp()
        => AppBuilder.Configure<App>()
            .UsePlatformDetect()
            .With(new AvaloniaNativePlatformOptions() { })
            .With(new X11PlatformOptions(){ })
            .LogToTrace();
}
