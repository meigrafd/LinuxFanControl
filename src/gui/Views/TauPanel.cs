using Gtk;

public class TauPanel : Box
{
    private readonly SpinButton _input;

    public TauPanel() : base(Orientation.Horizontal, 6)
    {
        _input = new SpinButton(0, 10, 0.1);
        _input.Value = 2.0;
        _input.SetCssClass("tile");

        Append(new Label(LocaleManager._("tau.label")));
        Append(_input);

        ApplyTheme();
    }

    private void ApplyTheme()
    {
        var css = $@"
        spinbutton.tile, label {{
            background-color: {ThemeManager.TileColor};
            color: {ThemeManager.TextColor};
            padding: 4px;
            border-radius: 4px;
        }}
        ";

        var provider = new CssProvider();
        provider.LoadFromData(css);
        StyleContext.AddProviderForDisplay(Display.Default, provider, 800);
    }
}
