using Gtk;
using System;
using FanControl.Gui;
using System.Linq;

namespace FanControl.Gui.Views;

public class SettingsView : VBox
{
    private ComboBoxText _themeSelector = new();
    private ComboBoxText _localeSelector = new();
    private Label _statusLabel = new();
    private Label _titleLabel = new();
    private Label _themeLabel = new();
    private Label _localeLabel = new();

    private UserSettings _settings;

    public SettingsView()
    {
        Spacing = 10;
        _settings = UserSettings.Load();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        UpdateStatus();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _titleLabel = new Label();
        _titleLabel.SetMarkup($"<b>{Translation.Get("sidebar.settings")}</b>");
        PackStart(_titleLabel, false, false, 0);

        _themeLabel = new Label(Translation.Get("settings.theme"));
        PackStart(_themeLabel, false, false, 0);

        _themeSelector = new ComboBoxText();
        var themes = ThemeManager.AvailableThemes();
        foreach (var theme in themes)
            _themeSelector.AppendText(theme);

        int themeIndex = Array.IndexOf(themes, _settings.Theme);
        _themeSelector.Active = themeIndex >= 0 ? themeIndex : 0;
        _themeSelector.Changed += OnSelectionChanged;
        PackStart(_themeSelector, false, false, 0);

        _localeLabel = new Label(Translation.Get("settings.locale"));
        PackStart(_localeLabel, false, false, 0);

        _localeSelector = new ComboBoxText();
        var locales = Translation.AvailableLocales();
        foreach (var locale in locales)
            _localeSelector.AppendText(locale);

        int localeIndex = Array.IndexOf(locales, _settings.Locale);
        _localeSelector.Active = localeIndex >= 0 ? localeIndex : 0;
        _localeSelector.Changed += OnSelectionChanged;
        PackStart(_localeSelector, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void OnSelectionChanged(object? sender, EventArgs e)
    {
        var selectedTheme = _themeSelector.ActiveText ?? "Light";
        var selectedLocale = _localeSelector.ActiveText ?? "en";

        bool themeChanged = selectedTheme != _settings.Theme;
        bool localeChanged = selectedLocale != _settings.Locale;

        _settings.Theme = selectedTheme;
        _settings.Locale = selectedLocale;
        _settings.Save();

        if (themeChanged)
            ThemeManager.Apply(selectedTheme);

        if (localeChanged)
            Translation.Load(selectedLocale);

        UpdateStatus(themeChanged, localeChanged);
    }

    private void UpdateStatus(bool themeChanged = false, bool localeChanged = false)
    {
        if (themeChanged || localeChanged)
        {
            string message = Translation.Get("settings.applied");
            if (themeChanged && localeChanged)
                message = Translation.Get("settings.applied.both");
            else if (themeChanged)
                message = Translation.Get("settings.applied.theme");
            else if (localeChanged)
                message = Translation.Get("settings.applied.locale");

            _statusLabel.Text = message;
        }
        else
        {
            string status = Translation.Get("settings.status")
            .Replace("{theme}", _settings.Theme)
            .Replace("{locale}", _settings.Locale);
            _statusLabel.Text = status;
        }
    }

    private void Redraw()
    {
        BuildUi();
        UpdateStatus();
    }
}
