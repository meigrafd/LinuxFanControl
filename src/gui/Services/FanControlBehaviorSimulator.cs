using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlBehaviorSimulator : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _profileSelector;
    private Entry _tempEntry;
    private Button _simulateButton;
    private TextView _resultView;

    public FanControlBehaviorSimulator()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadProfiles();
    }

    private void BuildUi()
    {
        _profileSelector = new ComboBoxText();
        PackStart(_profileSelector, false, false, 0);

        _tempEntry = new Entry { PlaceholderText = Translation.Get("simulate.temp") };
        PackStart(_tempEntry, false, false, 0);

        _simulateButton = new Button(Translation.Get("simulate.run"));
        _simulateButton.Clicked += (_, _) => RunSimulation();
        PackStart(_simulateButton, false, false, 0);

        _resultView = new TextView
        {
            Editable = false,
            WrapMode = WrapMode.Word
        };
        var scroll = new ScrolledWindow();
        scroll.Add(_resultView);
        scroll.SetSizeRequest(500, 200);
        PackStart(scroll, true, true, 0);
    }

    private void LoadProfiles()
    {
        _profileSelector.RemoveAll();

        var response = _rpc.SendRequest("listProfiles");
        if (response is JsonObject obj && obj["result"] is JsonArray profiles)
        {
            foreach (var p in profiles)
            {
                var name = p?.ToString() ?? "?";
                _profileSelector.AppendText(name);
            }

            if (_profileSelector.Children.Length > 0)
                _profileSelector.Active = 0;
        }
    }

    private void RunSimulation()
    {
        var profile = _profileSelector.ActiveText;
        var tempText = _tempEntry.Text;

        if (!string.IsNullOrEmpty(profile) && double.TryParse(tempText, out var temp))
        {
            var payload = new JsonObject
            {
                ["profile"] = profile,
                ["temperature"] = temp
            };
            var response = _rpc.SendRequest("simulateFanBehavior", payload);
            if (response is JsonObject obj && obj["result"] is JsonArray result)
            {
                var lines = string.Join("\n", result.Select(r => "• " + r?.ToString()));
                _resultView.Buffer.Text = Translation.Get("simulate.result") + "\n" + lines;
            }
            else
            {
                _resultView.Buffer.Text = Translation.Get("simulate.error");
            }
        }
        else
        {
            _resultView.Buffer.Text = Translation.Get("simulate.invalid");
        }
    }

    private void Redraw()
    {
        _simulateButton.Label = Translation.Get("simulate.run");
    }
}
