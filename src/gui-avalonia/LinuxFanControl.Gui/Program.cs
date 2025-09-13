// (c) 2025 LinuxFanControl contributors. MIT License.
using Avalonia;

namespace LinuxFanControl.Gui
{
    public static class Program
    {
        public static void Main(string[] args) =>
        BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);

        public static AppBuilder BuildAvaloniaApp() =>
        AppBuilder.Configure<App>()
        .UsePlatformDetect()
        .WithInterFont()
        .LogToTrace();
    }
}
