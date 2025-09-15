using Gtk;

public class StatusBar : Box
{
    private readonly Label _label;

    public StatusBar() : base(Orientation.Horizontal, 6)
    {
        _label = new Label(LocaleManager._("daemon.status.unknown"));
        _label.SetCssClass("tile");

        Append(_label);
        ApplyTheme();
    }

    public void SetText(string text)
    {
        _label.Text = text;
    }

    private void ApplyTheme()
    {
        var css = $@"
        label.tile {{
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
