// (c) 2025 LinuxFanControl contributors. MIT License.
#nullable enable
using Avalonia;
using Avalonia.ReactiveUI;
using Avalonia.Fonts.Inter;

namespace LinuxFanControl.Gui
{
    internal sealed class Program
    {
        public static void Main(string[] args)
        {
            BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
        }

        public static AppBuilder BuildAvaloniaApp()
        => AppBuilder.Configure<App>()
        .UsePlatformDetect()
        .WithInterFont()
        .LogToTrace()
        .UseReactiveUI();
    }
}
