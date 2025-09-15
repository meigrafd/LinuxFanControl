using Gtk;

public class SettingsPanel : Box
{
    private readonly ComboBoxText _langCombo;
    private readonly ComboBoxText _themeCombo;
    private readonly Label _status;

    public SettingsPanel(List<string> availableLanguages, List<string> availableThemes) : base(Orientation.Vertical, 12)
    {
        _langCombo = new ComboBoxText();
        _themeCombo = new ComboBoxText();
        _status = new Label();

        foreach (var lang in availableLanguages)
            _langCombo.AppendText(lang);

        foreach (var theme in availableThemes)
            _themeCombo.AppendText(theme);

        _langCombo.SetActiveId(SettingsManager.Get("language"));
        _themeCombo.SetActiveId(SettingsManager.Get("theme"));

        _langCombo.Changed += (_, _) =>
        {
            var lang = _langCombo.ActiveText;
            if (!string.IsNullOrEmpty(lang))
            {
                SettingsManager.Set("language", lang);
                UpdateStatus();
            }
        };

        _themeCombo.Changed += (_, _) =>
        {
            var theme = _themeCombo.ActiveText;
            if (!string.IsNullOrEmpty(theme))
            {
                SettingsManager.Set("theme", theme);
                UpdateStatus();
            }
        };

        Append(new Label(LocaleManager._("settings.locale")));
        Append(_langCombo);
        Append(new Label(LocaleManager._("settings.theme")));
        Append(_themeCombo);
        Append(_status);

        UpdateStatus();
    }

    private void UpdateStatus()
    {
        var text = LocaleManager._("settings.status")
        .Replace("{theme}", SettingsManager.Get("theme"))
        .Replace("{locale}", SettingsManager.Get("language"));

        _status.Text = text;
    }
}
