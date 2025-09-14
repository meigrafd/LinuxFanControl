using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Views;

public class StartupWizard : VBox
{
    private readonly RpcClient _rpc;

    private Label _introLabel;
    private Button _detectButton;
    private Button _startEngineButton;

    public StartupWizard()
    {
        Spacing = 12;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _introLabel = new Label(Translation.Get("wizard.intro"));
        PackStart(_introLabel, false, false, 0);

        _detectButton = new Button(Translation.Get("wizard.detect"));
        _detectButton.Clicked += (_, _) => RunDetection();
        PackStart(_detectButton, false, false, 0);

        _startEngineButton = new Button(Translation.Get("wizard.start"));
        _startEngineButton.Clicked += (_, _) => _rpc.SendRequest("engineStart");
        PackStart(_startEngineButton, false, false, 0);
    }

    private void RunDetection()
    {
        var result = _rpc.SendRequest("detectCalibrate");
        if (result is JsonObject obj && obj["result"] is JsonObject res)
        {
            var msg = Translation.Get("wizard.done");
            if (res["mapping"] is JsonArray map)
                msg += $"\n{map.Count} mappings created.";
            _introLabel.Text = msg;
        }
        else
        {
            _introLabel.Text = Translation.Get("wizard.error");
        }
    }

    private void Redraw()
    {
        BuildUi();
    }
}
