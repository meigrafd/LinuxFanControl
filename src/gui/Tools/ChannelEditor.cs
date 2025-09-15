using Gtk;

public class ChannelEditor : Box
{
    private readonly ComboBoxText _channelSelector;
    private readonly ComboBoxText _modeSelector;
    private readonly Scale _manualSlider;

    public ChannelEditor() : base(Orientation.Vertical, 6)
    {
        _channelSelector = new ComboBoxText();
        _modeSelector = new ComboBoxText();
        _manualSlider = new Scale(Orientation.Horizontal);

        _channelSelector.SetCssClass("tile");
        _modeSelector.SetCssClass("tile");
        _manualSlider.SetCssClass("tile");

        Append(_channelSelector);
        Append(_modeSelector);
        Append(_manualSlider);

        AddCss();
        LoadLocales();
    }

    private void LoadLocales()
    {
        _channelSelector.AppendText(LocaleManager._("channel.fan1"));
        _channelSelector.AppendText(LocaleManager._("channel.fan2"));
        _channelSelector.SetActive(0);

        _modeSelector.AppendText(LocaleManager._("mode.manual"));
        _modeSelector.AppendText(LocaleManager._("mode.curve"));
        _modeSelector.SetActive(0);

        _manualSlider.SetRange(0, 100);
        _manualSlider.Value = 50;
    }

    private void AddCss()
    {
        var css = @"
        combobox.tile, scale.tile {
            background-color: #1F2F4F;
            color: #FFFFFF;
            padding: 4px;
            border-radius: 4px;
        }
        ";

        var provider = new CssProvider();
        provider.LoadFromData(css);
        StyleContext.AddProviderForDisplay(Display.Default, provider, 800);
    }
}
