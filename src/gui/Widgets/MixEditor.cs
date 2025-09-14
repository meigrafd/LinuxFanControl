using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Widgets;

public class MixEditor : VBox
{
    private RpcClient _rpc;
    private string _mixName;

    private ComboBoxText _methodSelector;
    private Entry _outputNameEntry;
    private Button _applyButton;

    public MixEditor(string mixName)
    {
        Spacing = 10;
        _mixName = mixName;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _methodSelector = new ComboBoxText();
        _methodSelector.AppendText(Translation.Get("mix.method.max"));
        _methodSelector.AppendText(Translation.Get("mix.method.min"));
        _methodSelector.AppendText(Translation.Get("mix.method.avg"));
        _methodSelector.Active = 0;
        PackStart(_methodSelector, false, false, 0);

        _outputNameEntry = new Entry { PlaceholderText = Translation.Get("mix.output") };
        PackStart(_outputNameEntry, false, false, 0);

        _applyButton = new Button(Translation.Get("mix.add"));
        _applyButton.Clicked += (_, _) => ApplyMix();
        PackStart(_applyButton, false, false, 0);
    }

    private void ApplyMix()
    {
        string method = _methodSelector.ActiveText ?? "max";
        string outputName = _outputNameEntry.Text;

        var payload = new JsonObject
        {
            ["name"] = _mixName,
            ["method"] = method.ToLowerInvariant(),
            ["output"] = outputName
        };

        _rpc.SendRequest("createMix", payload);
    }

    private void Redraw()
    {
        BuildUi();
    }
}
