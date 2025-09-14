using Gtk;
using FanControl.Gui;
using System;

namespace FanControl.Gui.Views;

public class SettingsWindow : Window
{
    private readonly ComboBoxText _themeSelector;
    private readonly ComboBoxText _localeSelector;
    private readonly Label _statusLabel;

    public SettingsWindow() : base(Translation.T("sidebar.settings"))
    {
        SetDefaultSize(400, 200);
        SetPosition(WindowPosition.Center);

        var layout = new Box(Orientation.Vertical, 10)
        {
            Margin = 20
        };

        var themeLabel = new Label(Translation.T("settings.theme"));
        _themeSelector = new ComboBoxText();
        _themeSelector.AppendText("Light");
        _themeSelector.AppendText("Dark");
        _themeSelector.Active = 0;

        var localeLabel = new Label(Translation.T("settings.locale"));
        _localeSelector = new ComboBoxText();
        _localeSelector.AppendText("de");
        _localeSelector.AppendText("en");
        _localeSelector.Active = 0;

        _statusLabel = new Label("");

        _themeSelector.Changed += OnSelectionChanged;
        _localeSelector.Changed += OnSelectionChanged;

        layout.PackStart(themeLabel, false, false, 0);
        layout.PackStart(_themeSelector, false, false, 0);
        layout.PackStart(localeLabel, false, false, 0);
        layout.PackStart(_localeSelector, false, false, 0);
        layout.PackStart(_statusLabel, false, false, 0);

        Add(layout);
        ShowAll();

        UpdateStatus();
    }

    private void OnSelectionChanged(object? sender, EventArgs e)
    {
        string theme = _themeSelector.ActiveText ?? "Light";
        string locale = _localeSelector.ActiveText ?? "en";

        ThemeManager.Apply(theme);
        Translation.Load(locale);

        UpdateStatus();
    }

    private void UpdateStatus()
    {
        string theme = _themeSelector.ActiveText ?? "Light";
        string locale = _localeSelector.ActiveText ?? "en";

        _statusLabel.Text = Translation.T("settings.status")
        .Replace("{theme}", theme)
        .Replace("{locale}", locale);
    }
}
