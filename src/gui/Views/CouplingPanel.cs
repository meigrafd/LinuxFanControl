using Gtk;

public class CouplingPanel : Box
{
    private readonly ComboBoxText _sourceSelector;
    private readonly ComboBoxText _targetSelector;
    private readonly Button _deleteButton;

    public CouplingPanel() : base(Orientation.Horizontal, 12)
    {
        _sourceSelector = new ComboBoxText();
        _targetSelector = new ComboBoxText();
        _deleteButton = new Button(LocaleManager._("coupling.delete"));

        _sourceSelector.SetCssClass("tile");
        _targetSelector.SetCssClass("tile");
        _deleteButton.SetCssClass("tile");

        Append(_sourceSelector);
        Append(_targetSelector);
        Append(_deleteButton);

        AddCss();
        LoadLocales();
    }

    private void LoadLocales()
    {
        _sourceSelector.AppendText(LocaleManager._("sensor.1"));
        _sourceSelector.AppendText(LocaleManager._("sensor.2"));
        _sourceSelector.SetActive(0);

        _targetSelector.AppendText(LocaleManager._("fan.1"));
        _targetSelector.AppendText(LocaleManager._("fan.2"));
        _targetSelector.SetActive(0);
    }

    private void AddCss()
    {
        var css = @"
        combobox.tile, button.tile {
            background-color: #1F2F4F;
            color: #FFFFFF;
            padding: 6px;
            border-radius: 4px;
        }
        ";

        var provider = new CssProvider();
        provider.LoadFromData(css);
        StyleContext.AddProviderForDisplay(Display.Default, provider, 800);
    }
}
