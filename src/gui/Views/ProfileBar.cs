using Gtk;

public class ProfileBar : Box
{
    public ProfileBar() : base(Orientation.Horizontal, 12)
    {
        var loadButton = new Button(LocaleManager._("profile.load"));
        var saveButton = new Button(LocaleManager._("profile.save"));
        var deleteButton = new Button(LocaleManager._("profile.delete"));

        loadButton.SetCssClass("tile");
        saveButton.SetCssClass("tile");
        deleteButton.SetCssClass("tile");

        Append(loadButton);
        Append(saveButton);
        Append(deleteButton);

        ApplyTheme();
    }

    private void ApplyTheme()
    {
        var css = $@"
        button.tile {{
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
