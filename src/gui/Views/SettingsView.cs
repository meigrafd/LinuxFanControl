using Gtk;
using FanControl.Gui;
using System;

namespace FanControl.Gui.Views;

public class SettingsView : Box
{
    private readonly ComboBoxText _themeSelector;
    private readonly ComboBoxText _localeSelector;
    private readonly Label _statusLabel;

    public SettingsView() : base(Orientation.Vertical, 10)
    {
        Margin = 20;

        _themeSelector = new ComboBoxText();
        _themeSelector.AppendText("light");
        _themeSelector.AppendText("dark");
        _themeSelector.Active = 0;

        _localeSelector = new ComboBoxText();
        _localeSelector.AppendText("en");
        _localeSelector.AppendText("de");
        _localeSelector.Active = 0;

        var applyButton = new Button("Apply");
        applyButton.Clicked += (_, _) => ApplySettings();

        _statusLabel = new Label("");

        PackStart(new Label(Translation.T("settings.theme")), false, false, 0);
        PackStart(_themeSelector, false, false, 0);
        PackStart(new Label(Translation.T("settings.locale")), false, false, 0);
        PackStart(_localeSelector, false, false, 0);
        PackStart(applyButton, false, false, 0);
        PackStart(_statusLabel, false, false, 0);
    }

    private void ApplySettings()
    {
        string theme = _themeSelector.ActiveText ?? "light";
        string locale = _localeSelector.ActiveText ?? "en";

        ThemeManager.Apply(theme);
        Translation.Load(locale);

        _statusLabel.Text = Translation.T("settings.status")
        .Replace("{theme}", theme)
        .Replace("{locale}", locale);
    }
}
