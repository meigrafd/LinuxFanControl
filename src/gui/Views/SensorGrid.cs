using Gtk;
using System;
using System.Collections.Generic;
using System.Text;

public class SensorGrid : Box
{
    private readonly ShmReader _reader;
    private readonly Grid _grid;

    public SensorGrid() : base(Orientation.Vertical, 6)
    {
        _reader = new ShmReader();
        _reader.Init();

        _grid = new Grid();
        _grid.ColumnSpacing = 12;
        _grid.RowSpacing = 6;

        Append(_grid);
        ApplyTheme();

        GLib.Timeout.Add(500, () =>
        {
            Update();
            return true;
        });
    }

    private void Update()
    {
        var data = _reader.ReadSpan();
        if (data.IsEmpty) return;

        var sensors = ParseSensors(data);
        Render(sensors);
    }

    private List<(string label, float value, string source)> ParseSensors(Span<byte> data)
    {
        var result = new List<(string, float, string)>();
        int offset = 0;

        while (offset + 64 <= data.Length)
        {
            var label = Encoding.UTF8.GetString(data.Slice(offset, 32)).TrimEnd('\0');
            var source = Encoding.UTF8.GetString(data.Slice(offset + 32, 16)).TrimEnd('\0');
            var value = BitConverter.ToSingle(data.Slice(offset + 48, 4));

            result.Add((label, value, source));
            offset += 64;
        }

        return result;
    }

    private void Render(List<(string label, float value, string source)> sensors)
    {
        _grid.RemoveAll();

        int row = 0;
        foreach (var (label, value, source) in sensors)
        {
            var l1 = new Label(label);
            var l2 = new Label($"{value:F1} °C");
            var l3 = new Label(source);

            l1.SetCssClass("tile");
            l2.SetCssClass("tile");
            l3.SetCssClass("tile");

            _grid.Attach(l1, 0, row, 1, 1);
            _grid.Attach(l2, 1, row, 1, 1);
            _grid.Attach(l3, 2, row, 1, 1);

            row++;
        }

        _grid.ShowAll();
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
