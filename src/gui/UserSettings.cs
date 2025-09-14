using System.IO;
using System.Text.Json;

namespace FanControl.Gui;

public class UserSettings
{
    public string Locale { get; set; } = "en";
    public string Theme { get; set; } = "Light";

    private static readonly string Path = System.IO.Path.Combine("config", "lfc.json");

    public static UserSettings Load()
    {
        try
        {
            if (!File.Exists(Path))
                return new UserSettings();

            string json = File.ReadAllText(Path);
            return JsonSerializer.Deserialize<UserSettings>(json) ?? new UserSettings();
        }
        catch
        {
            return new UserSettings();
        }
    }

    public void Save()
    {
        Directory.CreateDirectory(System.IO.Path.GetDirectoryName(Path)!);
        string json = JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(Path, json);
    }
}
