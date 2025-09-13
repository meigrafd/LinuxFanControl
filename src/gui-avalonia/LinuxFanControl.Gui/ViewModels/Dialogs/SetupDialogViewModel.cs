using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels.Dialogs
{
    /// <summary>
    /// ViewModel for the initial Setup dialog: language, theme, and optional detection toggle.
    /// Plain INotifyPropertyChanged to avoid extra package dependencies.
    /// </summary>
    public class SetupDialogViewModel : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler? PropertyChanged;

        private string? _selectedLanguage;
        private string? _selectedTheme;
        private bool _runDetection;

        public ObservableCollection<string> AvailableLanguages { get; } = new();
        public ObservableCollection<string> AvailableThemes { get; } = new();

        public string? SelectedLanguage
        {
            get => _selectedLanguage;
            set
            {
                if (SetField(ref _selectedLanguage, value) && !string.IsNullOrWhiteSpace(value))
                    LocalizationService.SetLanguage(value!);
            }
        }

        public string? SelectedTheme
        {
            get => _selectedTheme;
            set
            {
                if (SetField(ref _selectedTheme, value) && !string.IsNullOrWhiteSpace(value))
                    ThemeManager.ApplyTheme(value!);
            }
        }

        public bool RunDetection
        {
            get => _runDetection;
            set => SetField(ref _runDetection, value);
        }

        public SetupDialogViewModel()
        {
            // dynamic discovery (no hardcoding)
            AvailableLanguages.Clear();
            foreach (var l in LocalizationService.ListLanguages())
                AvailableLanguages.Add(l);

            AvailableThemes.Clear();
            foreach (var t in ThemeManager.ListThemes())
                AvailableThemes.Add(t);

            var (lang, theme) = LocalizationService.LoadGuiConfigOrDefault();
            _selectedLanguage = lang;
            _selectedTheme = theme;
            _runDetection = false; // explicit opt-in
        }

        public void SavePreferences()
        {
            var lang = _selectedLanguage ?? LocalizationService.DefaultLanguage;
            var theme = _selectedTheme ?? ThemeManager.DefaultTheme;
            LocalizationService.SaveGuiConfig(lang, theme);
        }

        private bool SetField<T>(ref T field, T value, [CallerMemberName] string? name = null)
        {
            if (Equals(field, value)) return false;
            field = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
            return true;
        }
    }
}
