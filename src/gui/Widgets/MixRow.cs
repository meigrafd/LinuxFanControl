using Gtk;
using System;
using FanControl.Gui.Views;

namespace FanControl.Gui.Widgets;

public class MixRow : Box
{
    private readonly ComboBoxText _sensorA;
    private readonly ComboBoxText _sensorB;
    private readonly ComboBoxText _methodSelector;
    private readonly Entry _outputName;
    private readonly Button _removeButton;

    public MixRow() : base(Orientation.Horizontal, 5)
    {
        _sensorA = new ComboBoxText();
        _sensorA.AppendText("CPU");
        _sensorA.AppendText("GPU");
        _sensorA.AppendText("Mainboard");
        _sensorA.Active = 0;

        _sensorB = new ComboBoxText();
        _sensorB.AppendText("CPU");
        _sensorB.AppendText("GPU");
        _sensorB.AppendText("Mainboard");
        _sensorB.Active = 1;

        _methodSelector = new ComboBoxText();
        _methodSelector.AppendText(Translation.Get("mix.method.max"));
        _methodSelector.AppendText(Translation.Get("mix.method.min"));
        _methodSelector.AppendText(Translation.Get("mix.method.avg"));
        _methodSelector.Active = 0;

        _outputName = new Entry { PlaceholderText = Translation.Get("mix.output") };

        _removeButton = new Button("✕");
        _removeButton.TooltipText = Translation.Get("mix.remove");
        _removeButton.Clicked += (_, _) => this.Destroy();

        PackStart(_sensorA, false, false, 0);
        PackStart(_methodSelector, false, false, 0);
        PackStart(_sensorB, false, false, 0);
        PackStart(_outputName, true, true, 0);
        PackStart(_removeButton, false, false, 0);
    }

    public string SensorA => _sensorA.ActiveText ?? "";
    public string SensorB => _sensorB.ActiveText ?? "";
    public string Method => _methodSelector.ActiveText ?? "";
    public string OutputName => _outputName.Text;
}
