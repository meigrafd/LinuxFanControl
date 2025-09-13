using System.Collections.ObjectModel;
using System.Linq;
using System.Reactive;
using ReactiveUI;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels.Dialogs
{
    public class SetupDialogViewModel : ReactiveObject
    {
        public ObservableCollection<string> Themes { get; } = new();
        public ObservableCollection<LocalizationService.LanguageItem> Languages { get; } = new();

        private string? _selectedTheme;
        public string? SelectedTheme
        {
            get => _selectedTheme;
            set => this.RaiseAndSetIfChanged(ref _selectedTheme, value);
        }

        private LocalizationService.LanguageItem? _selectedLanguage;
        public LocalizationService.LanguageItem? SelectedLanguage
        {
            get => _selectedLanguage;
            set => this.RaiseAndSetIfChanged(ref _selectedLanguage, value);
        }

        public bool RunDetection { get; set; } = true;
        public bool RunCalibration { get; set; } = false;

        public ReactiveCommand<Unit, Unit> ApplyCommand { get; }
        public ReactiveCommand<Unit, Unit> CancelCommand { get; }

        public SetupDialogViewModel()
        {
            // Fill themes
            foreach (var t in ThemeManager.ListThemes())
                Themes.Add(t);
            SelectedTheme = Themes.Contains(ThemeManager.CurrentTheme)
            ? ThemeManager.CurrentTheme
            : Themes.FirstOrDefault() ?? ThemeManager.DefaultTheme;

            // Fill languages
            foreach (var l in LocalizationService.ListLanguages())
                Languages.Add(l);
            var cur = Languages.FirstOrDefault(x => x.Code == LocalizationService.CurrentLanguage)
            ?? Languages.FirstOrDefault();
            SelectedLanguage = cur;

            ApplyCommand = ReactiveCommand.Create(() =>
            {
                if (!string.IsNullOrWhiteSpace(SelectedTheme))
                    ThemeManager.ApplyTheme(SelectedTheme!);
                if (SelectedLanguage != null)
                    LocalizationService.SetLanguage(SelectedLanguage.Code);
                // Detection flags are read by the dialog’s code-behind after close
            });

            CancelCommand = ReactiveCommand.Create(() => { /* nothing, dialog will close */ });
        }
    }
}
