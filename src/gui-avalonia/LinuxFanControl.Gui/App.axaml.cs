using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using LinuxFanControl.Gui.Services;
using LinuxFanControl.Gui.Views;

namespace LinuxFanControl.Gui
{
    public partial class App : Application
    {
        public override void Initialize()
        {
            AvaloniaXamlLoader.Load(this);
        }

        public override void OnFrameworkInitializationCompleted()
        {
            if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            {
                // Load persisted GUI config
                var cfg = ConfigService.Load();
                // Apply localization and theme
                LocalizationService.SetLocale(cfg.Language);
                ThemeManager.ApplyTheme(cfg.Theme);

                desktop.MainWindow = new MainWindow();
            }
            base.OnFrameworkInitializationCompleted();
        }
    }
}
