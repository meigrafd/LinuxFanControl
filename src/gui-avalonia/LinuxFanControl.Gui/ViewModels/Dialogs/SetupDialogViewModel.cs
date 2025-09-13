using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Runtime.CompilerServices;
using System.Windows.Input;
using Avalonia.Controls;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels.Dialogs
{
    // ViewModel for the Setup dialog – provides localized labels, theme/locale lists, and actions.
    public sealed class SetupDialogViewModel : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler? PropertyChanged;
        void Raise([CallerMemberName] string? n = null) => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(n));

        // Labels (localized)
        public string Title { get; private set; } = "Setup";
        public string Header { get; private set; } = "First-time Setup";
        public string SubHeader { get; private set; } = "Choose theme & language, optionally run detection.";
        public string ThemeLabel { get; private set; } = "Theme";
        public string LanguageLabel { get; private set; } = "Language";
        public string DetectionLabel { get; private set; } = "Run detection on close";
        public string DetectionHint { get; private set; } = "Probe sensors & PWM automatically after closing.";
        public string ImportLabel { get; private set; } = "Import FanControl config";
        public string ImportBrowseButton { get; private set; } = "Browse…";
        public string CancelText { get; private set; } = "Cancel";
        public string ApplyText { get; private set; } = "Apply";

        // Choices
        public ObservableCollection<string> AvailableThemes { get; } = new();
        public ObservableCollection<string> AvailableLocales { get; } = new();

        // Current selection
        string _selectedTheme = "";
        public string SelectedTheme { get => _selectedTheme; set { _selectedTheme = value; Raise(); } }

        string _selectedLocale = "";
        public string SelectedLocale { get => _selectedLocale; set { _selectedLocale = value; Raise(); } }

        bool _runDetectionOnClose;
        public bool RunDetectionOnClose { get => _runDetectionOnClose; set { _runDetectionOnClose = value; Raise(); } }

        string _importPath = "";
        public string ImportPath { get => _importPath; set { _importPath = value; Raise(); } }

        // Commands
        public ICommand BrowseImportCommand { get; }
        public ICommand ApplyCommand { get; }
        public ICommand CancelCommand { get; }

        readonly Window _owner;

        public SetupDialogViewModel(Window owner)
        {
            _owner = owner;

            // Load strings (fallback = key)
            void L()
            {
                Title              = LocalizationService.Get("setup.title");
                Header             = LocalizationService.Get("setup.header");
                SubHeader          = LocalizationService.Get("setup.subheader");
                ThemeLabel         = LocalizationService.Get("setup.theme");
                LanguageLabel      = LocalizationService.Get("setup.language");
                DetectionLabel     = LocalizationService.Get("setup.detect");
                DetectionHint      = LocalizationService.Get("setup.detect_hint");
                ImportLabel        = LocalizationService.Get("setup.import");
                ImportBrowseButton = LocalizationService.Get("setup.import_browse");
                CancelText         = LocalizationService.Get("ui.cancel");
                ApplyText          = LocalizationService.Get("ui.apply");
                Raise(nameof(Title)); Raise(nameof(Header)); Raise(nameof(SubHeader));
                Raise(nameof(ThemeLabel)); Raise(nameof(LanguageLabel)); Raise(nameof(DetectionLabel));
                Raise(nameof(DetectionHint)); Raise(nameof(ImportLabel)); Raise(nameof(ImportBrowseButton));
                Raise(nameof(CancelText)); Raise(nameof(ApplyText));
            }
            L();

            // Fill theme list (from disk)
            AvailableThemes.Clear();
            foreach (var t in ThemeManager.ListThemes())
                AvailableThemes.Add(t);
            SelectedTheme = ThemeManager.CurrentTheme ?? (AvailableThemes.Count > 0 ? AvailableThemes[0] : "");

            // Fill locale list (from disk)
            AvailableLocales.Clear();
            foreach (var loc in LocalizationService.ListLocales())
                AvailableLocales.Add(loc);
            SelectedLocale = LocalizationService.CurrentLocale ?? (AvailableLocales.Count > 0 ? AvailableLocales[0] : "en");

            // Commands
            BrowseImportCommand = new RelayCommand(async _ =>
            {
                var dlg = new OpenFileDialog
                {
                    Title = LocalizationService.Get("setup.import_browse"),
                                                   AllowMultiple = false,
                                                   Filters =
                                                   {
                                                       new FileDialogFilter { Name = "JSON", Extensions = { "json" } },
                                                       new FileDialogFilter { Name = "All files", Extensions = { "*" } }
                                                   }
                };
                var files = await dlg.ShowAsync(_owner);
                if (files is { Length: > 0 })
                    ImportPath = files[0];
            });

            ApplyCommand = new RelayCommand(_ =>
            {
                // Apply theme & locale
                if (!string.IsNullOrWhiteSpace(SelectedTheme))
                    ThemeManager.ApplyTheme(SelectedTheme);

                if (!string.IsNullOrWhiteSpace(SelectedLocale))
                    LocalizationService.Load(SelectedLocale);

                // Persist GUI config
                ThemeManager.SaveGuiConfig(SelectedTheme, SelectedLocale);

                // Close dialog with OK
                (_owner as IDialogHost)?.CloseDialog(true, RunDetectionOnClose, ImportPath);
            });

            CancelCommand = new RelayCommand(_ =>
            {
                (_owner as IDialogHost)?.CloseDialog(false, false, "");
            });
        }

        // Simple ICommand implementation
        private sealed class RelayCommand : ICommand
        {
            readonly Action<object?> _action;
            public RelayCommand(Action<object?> action) => _action = action;
            public bool CanExecute(object? parameter) => true;
            public void Execute(object? parameter) => _action(parameter);
            public event EventHandler? CanExecuteChanged { add { } remove { } }
        }
    }

    // Dialog host interface – the Window implements this to receive results.
    public interface IDialogHost
    {
        void CloseDialog(bool ok, bool runDetection, string importPath);
    }
}
