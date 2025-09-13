using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using LinuxFanControl.Gui.Services;
using System.Collections.ObjectModel;
using System.Threading.Tasks;

namespace LinuxFanControl.Gui.ViewModels;

public partial class MainWindowViewModel : ObservableObject
{
    public LocalizationService Loc { get; } = new();
    public ObservableCollection<FanTileVm> FanTiles { get; } = new();
    public ObservableCollection<TriggerVm> Triggers { get; } = new();
    public string[] Languages { get; } = new []{ "en","de" };

    [ObservableProperty] private string selectedLanguage = "en";
    partial void OnSelectedLanguageChanged(string value){ Loc.Use(value); }

    [ObservableProperty] private bool isDarkTheme = true;
    public string ThemeLabel => Loc["Theme"];

    private RpcClient _rpc = new();

    public MainWindowViewModel()
    {
        _ = RefreshAsync();
    }

    [RelayCommand]
    private async Task RefreshAsync()
    {
        var inv = await _rpc.BatchAsync(new(){
            new RpcCall("listPwms"), new RpcCall("listSensors")
        });
        var pwms = inv["listPwms"].AsArray();
        FanTiles.Clear();
        foreach(var p in pwms)
        {
            var label = p.GetProperty("label").GetString() ?? "pwm";
            FanTiles.Add(new FanTileVm{ Title=label, Subtitle=p.GetProperty("pwm_path").GetString() ?? "", Duty=0, TelemetryLine="--" });
        }
    }

    [RelayCommand]
    private async Task SetupAsync()
    {
        await _rpc.CallAsync("detectCalibrate");
        await RefreshAsync();
    }
}

public class FanTileVm : ObservableObject
{
    public string Title { get; set; } = "";
    public string Subtitle { get; set; } = "";
    public double Duty { get; set; }
    public string TelemetryLine { get; set; } = "";
}
public class TriggerVm : ObservableObject
{
    public string Name { get; set; } = "";
    public string Summary { get; set; } = "";
}
