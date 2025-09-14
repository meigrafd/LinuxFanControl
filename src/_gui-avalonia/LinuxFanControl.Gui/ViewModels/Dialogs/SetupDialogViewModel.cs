using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using ReactiveUI;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels.Dialogs
{
    public sealed class SetupDialogViewModel : ReactiveObject
    {
        public ObservableCollection<string> Languages { get; } = new();
        public ObservableCollection<string> Themes { get; } = new();

        string _selectedLanguage = "";
        public string SelectedLanguage
        {
            get => _selectedLanguage;
            set => this.RaiseAndSetIfChanged(ref _selectedLanguage, value);
        }

        string _selectedTheme = "";
        public string SelectedTheme
        {
            get => _selectedTheme;
            set => this.RaiseAndSetIfChanged(ref _selectedTheme, value);
        }

        bool _runDetection;
        public bool RunDetection
        {
            get => _runDetection;
            set => this.RaiseAndSetIfChanged(ref _runDetection, value);
        }

        // localized labels
        public string L_Language => LocalizationService.Instance.T("ui.language");
        public string L_Theme    => LocalizationService.Instance.T("ui.theme");
        public string L_Detect   => LocalizationService.Instance.T("ui.setup.run_detection");
        public string L_Apply    => LocalizationService.Instance.T("ui.apply");
        public string L_Cancel   => LocalizationService.Instance.T("ui.cancel");

        public SetupDialogViewModel()
        {
            Languages.Clear();
            foreach (var l in LocalizationService.Instance.ListLanguages()) Languages.Add(l);
            SelectedLanguage = LocalizationService.Instance.CurrentLanguage;

            Themes.Clear();
            foreach (var t in ThemeManager.ListThemes()) Themes.Add(t);
            SelectedTheme = ThemeManager.CurrentTheme.Length > 0 ? ThemeManager.CurrentTheme : ThemeManager.DefaultTheme();

            LocalizationService.Instance.LanguageChanged += () =>
            {
                // notify relocalization
                this.RaisePropertyChanged(nameof(L_Language));
                this.RaisePropertyChanged(nameof(L_Theme));
                this.RaisePropertyChanged(nameof(L_Detect));
                this.RaisePropertyChanged(nameof(L_Apply));
                this.RaisePropertyChanged(nameof(L_Cancel));
            };
        }

        public async Task<bool> ApplyAsync()
        {
            try
            {
                // apply theme first (so dialog updates instantly)
                ThemeManager.Apply(SelectedTheme);

                // then language
                LocalizationService.Instance.Load(SelectedLanguage);

                // optionally trigger detection later (GUI will call into daemon)
                return true;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"[setup] apply failed: {ex}");
                return false;
            }
        }
    }
}
