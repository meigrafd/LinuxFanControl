using System.Text.Json;

namespace LinuxFanControl.Gui.Services;

public class LocalizationService
{
    private Dictionary<string,string> _dict = new();
    public LocalizationService(){ Use("en"); }
    public string this[string key] => _dict.TryGetValue(key, out var v) ? v : key;
    public void Use(string lang)
    {
        var path = Path.Combine(AppContext.BaseDirectory, "i18n", lang, "strings.json");
        if(!File.Exists(path)){
            _dict = new(){ ["Setup"]="Setup", ["Refresh"]="Refresh", ["Theme"]="Theme", ["Triggers"]="Triggers" };
            return;
        }
        var json = File.ReadAllText(path);
        var doc = JsonDocument.Parse(json);
        _dict = doc.RootElement.EnumerateObject().ToDictionary(p=>p.Name, p=>p.Value.GetString() ?? p.Name);
    }
    public string Setup => this["Setup"];
    public string Refresh => this["Refresh"];
    public string Theme => this["Theme"];
    public string Triggers => this["Triggers"];
}
