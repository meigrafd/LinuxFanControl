using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels.Dialogs
{
    /// <summary>
    /// ViewModel for the initial setup dialog (theme, language, detection calibration option).
    /// No ReactiveUI required; plain INotifyPropertyChanged for robust bindings.
    /// </summary>
    public class SetupDialogViewModel : INotifyPropertyChanged
    {
        private string? _selectedLanguage;
        private string? _selectedTheme;
        private bool _runDetection;

        public event PropertyChangedEventHandler? PropertyChanged;

        public ObservableCollection<string> AvailableLanguages { get; } = new();
        public ObservableCollection<string> AvailableThemes { get; } = new();

        public string? SelectedLanguage
        {
            get => _selectedLanguage;
            set
            {
                if (SetField(ref _selectedLanguage, value))
                {
                    if (!string.IsNullOrWhiteSpace(value))
                        LocalizationService.SetLanguage(value!); // apply immediately
                }
            }
        }

        public string? SelectedTheme
        {
            get => _selectedTheme;
            set
            {
                if (SetField(ref _selectedTheme, value))
                {
                    if (!string.IsNullOrWhiteSpace(value))
                        ThemeManager.ApplyTheme(value!); // apply immediately
                }
            }
        }

        public bool RunDetection
        {
            get => _runDetection;
            set => SetField(ref _runDetection, value);
        }

        public SetupDialogViewModel()
        {
            // Populate from services (dynamic discovery, no hardcoding)
            var langs = LocalizationService.ListLanguages();
            AvailableLanguages.Clear();
            foreach (var l in langs) AvailableLanguages.Add(l);

            var themes = ThemeManager.ListThemes();
            AvailableThemes.Clear();
            foreach (var t in themes) AvailableThemes.Add(t);

            // Defaults from persisted GUI config (if any)
            var (lang, theme) = LocalizationService.LoadGuiConfigOrDefault();
            _selectedLanguage = lang;
            _selectedTheme = theme;

            // Sensible default: do not auto-run detection unless user opts in
            _runDetection = false;
        }

        public void SavePreferences()
        {
            var lang = _selectedLanguage ?? LocalizationService.DefaultLanguage;
            var theme = _selectedTheme ?? ThemeManager.DefaultTheme;
            LocalizationService.SaveGuiConfig(lang, theme);
        }

        protected bool SetField<T>(ref T field, T value, [CallerMemberName] string? prop = null)
        {
            if (RuntimeHelpers.Equals(field, value))
                return false;
            field = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(prop));
            return true;
        }
    }
}
