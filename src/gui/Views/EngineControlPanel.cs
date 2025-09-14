using Gtk;
using FanControl.Gui.Services;

namespace FanControl.Gui.Views;

public class EngineControlPanel : HBox
{
    private readonly RpcClient _rpc;
    private Button _startButton;
    private Button _stopButton;

    public EngineControlPanel()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _startButton = new Button(Translation.Get("engine.start"));
        _startButton.Clicked += (_, _) => _rpc.SendRequest("engineStart");
        PackStart(_startButton, false, false, 0);

        _stopButton = new Button(Translation.Get("engine.stop"));
        _stopButton.Clicked += (_, _) => _rpc.SendRequest("engineStop");
        PackStart(_stopButton, false, false, 0);
    }

    private void Redraw()
    {
        _startButton.Label = Translation.Get("engine.start");
        _stopButton.Label = Translation.Get("engine.stop");
    }
}
