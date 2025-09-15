using Gtk;

public class ProfileSelector : Box
{
    private readonly ComboBoxText _combo;
    private readonly Button _apply;

    public ProfileSelector() : base(Orientation.Horizontal, 12)
    {
        _combo = new ComboBoxText();
        _apply = new Button(LocaleManager._("profile.apply"));

        _combo.SetCssClass("tile");
        _apply.SetCssClass("tile");

        Append(_combo);
        Append(_apply);

        ApplyTheme();
    }

    public void LoadProfiles(List<string> names)
    {
        _combo.RemoveAll();
        foreach (var name in names)
            _combo.AppendText(name);

        if (names.Count > 0)
            _combo.SetActive(0);
    }

    private void ApplyTheme()
    {
        var css = $@"
        combobox.tile, button.tile {{
            background-color: {ThemeManager.TileColor};
            color: {ThemeManager.TextColor};
            padding: 6px;
            border-radius: 4px;
        }}
        ";

        var provider = new CssProvider();
        provider.LoadFromData(css);
        StyleContext.AddProviderForDisplay(Display.Default, provider, 800);
    }
}
