using Gtk;

public class EngineControl : Box
{
    private readonly Button _startButton;
    private readonly Button _stopButton;

    public EngineControl() : base(Orientation.Horizontal, 12)
    {
        _startButton = new Button(LocaleManager._("engine.start"));
        _stopButton = new Button(LocaleManager._("engine.stop"));

        _startButton.SetCssClass("tile");
        _stopButton.SetCssClass("tile");

        Append(_startButton);
        Append(_stopButton);

        ApplyTheme();
    }

    public void Enable()
    {
        _startButton.Sensitive = true;
        _stopButton.Sensitive = true;
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
