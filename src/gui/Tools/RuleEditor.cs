using Gtk;
using System.Collections.Generic;

public class RuleEditor : Box
{
    private readonly Grid _grid;

    public RuleEditor() : base(Orientation.Vertical, 6)
    {
        _grid = new Grid();
        _grid.ColumnSpacing = 12;
        _grid.RowSpacing = 6;

        Append(_grid);
        ApplyTheme();
        Render();
    }

    private void Render()
    {
        for (int i = 0; i < 4; i++)
        {
            var trigger = new ComboBoxText();
            var target = new ComboBoxText();
            var curve = new Scale(Orientation.Horizontal);
            var hyst = new SpinButton(0, 10, 0.1);
            var tau = new SpinButton(0, 10, 0.1);

            trigger.AppendText(LocaleManager._("sensor.1"));
            trigger.AppendText(LocaleManager._("sensor.2"));
            trigger.SetActive(0);

            target.AppendText(LocaleManager._("fan.1"));
            target.AppendText(LocaleManager._("fan.2"));
            target.SetActive(0);

            curve.SetRange(0, 100);
            curve.Value = 50;

            hyst.Value = 1.0;
            tau.Value = 2.0;

            trigger.SetCssClass("tile");
            target.SetCssClass("tile");
            curve.SetCssClass("tile");
            hyst.SetCssClass("tile");
            tau.SetCssClass("tile");

            _grid.Attach(trigger, 0, i, 1, 1);
            _grid.Attach(target, 1, i, 1, 1);
            _grid.Attach(curve, 2, i, 1, 1);
            _grid.Attach(hyst, 3, i, 1, 1);
            _grid.Attach(tau, 4, i, 1, 1);
        }

        _grid.ShowAll();
    }

    private void ApplyTheme()
    {
        var css = $@"
        combobox.tile, scale.tile, spinbutton.tile {{
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

    public void LoadChannels(JsonNode? data)
    {
        // Placeholder for dynamic channel injection
    }
}
