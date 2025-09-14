#nullable enable
// (c) 2025 LinuxFanControl contributors. MIT License.
using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using LinuxFanControl.Gui;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui
{
    public partial class App : Application
    {
        public override void Initialize()
        {
            AvaloniaXamlLoader.Load(this);

            var assetsRoot = AssetLocator.GetAssetsRoot();
            LoadLocales(assetsRoot);
            LoadThemes(assetsRoot);
        }

        public override void OnFrameworkInitializationCompleted()
        {
            if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            {
                desktop.MainWindow = new MainWindow();
            }

            base.OnFrameworkInitializationCompleted();
        }

        private void LoadLocales(string root)
        {
            // TODO: JSON-Dateien aus root/Locales laden und als ResourceDictionary registrieren
        }

        private void LoadThemes(string root)
        {
            var themes = ThemeManager.ListThemes(root);
            if (themes.Length > 0)
            {
                ThemeManager.ApplyTheme(root, themes[0]);
            }
        }
    }
}
