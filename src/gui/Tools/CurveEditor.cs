using Gtk;

public class CurveEditor : Box
{
    private readonly Scale _point1;
    private readonly Scale _point2;
    private readonly Scale _point3;

    public CurveEditor() : base(Orientation.Vertical, 6)
    {
        _point1 = new Scale(Orientation.Horizontal);
        _point2 = new Scale(Orientation.Horizontal);
        _point3 = new Scale(Orientation.Horizontal);

        _point1.SetRange(0, 100);
        _point2.SetRange(0, 100);
        _point3.SetRange(0, 100);

        _point1.Value = 30;
        _point2.Value = 60;
        _point3.Value = 90;

        _point1.SetCssClass("tile");
        _point2.SetCssClass("tile");
        _point3.SetCssClass("tile");

        Append(new Label(LocaleManager._("curve.point1")));
        Append(_point1);
        Append(new Label(LocaleManager._("curve.point2")));
        Append(_point2);
        Append(new Label(LocaleManager._("curve.point3")));
        Append(_point3);

        ApplyTheme();
    }

    private void ApplyTheme()
    {
        var css = $@"
        scale.tile, label {{
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
