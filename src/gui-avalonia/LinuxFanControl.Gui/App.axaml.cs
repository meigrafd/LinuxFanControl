using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Avalonia.Themes.Fluent;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui
{
    public partial class App : Application
    {
        public override void Initialize()
        {
            // Fluent base so controls look modern
            Styles.Insert(0, new FluentTheme());

            AvaloniaXamlLoader.Load(this);

            // Apply persisted theme if you already save it; otherwise default "midnight".
            // Replace with your config read if available.
            ThemeManager.ApplyTheme(ThemeManager.DefaultThemeName);
        }

        public override void OnFrameworkInitializationCompleted()
        {
            if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            {
                if (desktop.MainWindow == null)
                {
                    desktop.MainWindow = new Views.MainWindow();
                }
            }

            base.OnFrameworkInitializationCompleted();
        }
    }
}
