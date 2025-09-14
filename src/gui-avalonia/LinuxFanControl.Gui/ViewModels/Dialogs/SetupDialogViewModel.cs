// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using System.Linq;
using ReactiveUI;
using LinuxFanControl.Gui.Services;
using LinuxFanControl.Gui.ViewModels;

namespace LinuxFanControl.Gui.ViewModels.Dialogs
{
    public partial class SetupDialogViewModel : ViewModelBase
    {
        public string[] Themes { get; }

        private string _selectedTheme = string.Empty;
        public string SelectedTheme
        {
            get => _selectedTheme;
            set => this.RaiseAndSetIfChanged(ref _selectedTheme, value);
        }

        public SetupDialogViewModel()
        {
            var assetsRoot = AssetLocator.GetAssetsRoot();
            Themes = ThemeManager.ListThemes(assetsRoot);
            SelectedTheme = Themes.Length > 0
            ? Themes[0]
            : string.Empty;
        }
    }
}
