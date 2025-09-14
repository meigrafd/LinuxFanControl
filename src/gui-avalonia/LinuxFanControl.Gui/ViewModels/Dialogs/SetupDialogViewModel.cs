using System.Collections.ObjectModel;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels.Dialogs
{
    public class SetupDialogViewModel
    {
        public ObservableCollection<string> Languages { get; } = new();
        public ObservableCollection<string> Themes { get; } = new();
        public string SelectedLanguage { get; set; } = "en";
        public string SelectedTheme { get; set; } = "midnight";

        public SetupDialogViewModel()
        {
            foreach (var l in LocalizationService.ListLanguages()) Languages.Add(l);
            foreach (var t in ThemeManager.ListThemes()) Themes.Add(t);
            SelectedLanguage = LocalizationService.CurrentLanguage;
        }
    }
}
