using System.Collections.Generic;
using System.IO;
using System.Text.Json;

namespace FanControl.Gui.Services;

public static class CurveStorage
{
    private static readonly string CurveFilePath = Path.Combine(
        System.Environment.GetFolderPath(System.Environment.SpecialFolder.ApplicationData),
                                                                "FanControl",
                                                                "curve.json"
    );

    public static void Save(List<(double x, double y)> points)
    {
        var data = new List<PointDto>();
        foreach (var (x, y) in points)
            data.Add(new PointDto { X = x, Y = y });

        var json = JsonSerializer.Serialize(data, new JsonSerializerOptions
        {
            WriteIndented = true
        });

        Directory.CreateDirectory(Path.GetDirectoryName(CurveFilePath)!);
        File.WriteAllText(CurveFilePath, json);
    }

    public static List<(double x, double y)> Load()
    {
        if (!File.Exists(CurveFilePath))
            return new List<(double x, double y)>();

        var json = File.ReadAllText(CurveFilePath);
        var data = JsonSerializer.Deserialize<List<PointDto>>(json) ?? new();

        var result = new List<(double x, double y)>();
        foreach (var point in data)
            result.Add((point.X, point.Y));

        return result;
    }

    private class PointDto
    {
        public double X { get; set; }
        public double Y { get; set; }
    }
}
