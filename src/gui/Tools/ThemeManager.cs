using System.Text.Json.Nodes;

public static class ThemeManager
{
    public static string BackgroundColor { get; private set; } = "#000000";
    public static string TileColor       { get; private set; } = "#222222";
    public static string TextColor       { get; private set; } = "#FFFFFF";
    public static string AccentColor     { get; private set; } = "#FFAA00";

    public static void Load(string themeName)
    {
        var path = Path.Combine(AppContext.BaseDirectory, "Themes", $"{themeName}.json");

        if (!File.Exists(path))
            throw new FileNotFoundException($"Theme file not found: {path}");

        var json = File.ReadAllText(path);
        var node = JsonNode.Parse(json);

        BackgroundColor = node?["background"]?.ToString() ?? BackgroundColor;
        TileColor       = node?["tile"]?.ToString()       ?? TileColor;
        TextColor       = node?["text"]?.ToString()       ?? TextColor;
        AccentColor     = node?["accent"]?.ToString()     ?? AccentColor;
    }
}
