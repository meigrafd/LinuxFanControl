// (c) 2025 LinuxFanControl contributors. MIT License.
using System.Collections.ObjectModel;
using System.Linq;
using CommunityToolkit.Mvvm.ComponentModel;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels
{
    public partial class MainWindowViewModel : ObservableObject
    {
        [ObservableProperty] private ObservableCollection<ThemeManager.ThemeInfo> themes = new();
        [ObservableProperty] private ThemeManager.ThemeInfo? selectedTheme;

        partial void OnSelectedThemeChanged(ThemeManager.ThemeInfo? value)
        {
            if (value is null) return;
            ThemeManager.Instance.Apply(value);
        }

        public void EnsureThemesInitialized()
        {
            Themes.Clear();
            foreach (var t in ThemeManager.Instance.Themes)
                Themes.Add(t);
            SelectedTheme = ThemeManager.Instance.Current ?? Themes.FirstOrDefault();
        }
    }
}
