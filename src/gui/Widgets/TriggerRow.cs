using Gtk;
using System;
using FanControl.Gui.Views;

namespace FanControl.Gui.Widgets;

public class TriggerRow : Box
{
    private readonly ComboBoxText _sensorSelector;
    private readonly SpinButton _thresholdInput;
    private readonly ComboBoxText _directionSelector;
    private readonly Entry _profileInput;
    private readonly Button _removeButton;

    public TriggerRow() : base(Orientation.Horizontal, 5)
    {
        _sensorSelector = new ComboBoxText();
        _sensorSelector.AppendText("CPU");
        _sensorSelector.AppendText("GPU");
        _sensorSelector.AppendText("Mainboard");
        _sensorSelector.Active = 0;

        _thresholdInput = new SpinButton(0, 100, 0.5) { Value = 60 };

        _directionSelector = new ComboBoxText();
        _directionSelector.AppendText(Translation.Get("trigger.direction.greater"));
        _directionSelector.AppendText(Translation.Get("trigger.direction.less"));
        _directionSelector.Active = 0;

        _profileInput = new Entry { PlaceholderText = Translation.Get("trigger.target") };

        _removeButton = new Button("✕");
        _removeButton.TooltipText = Translation.Get("trigger.remove");
        _removeButton.Clicked += (_, _) => this.Destroy();

        PackStart(_sensorSelector, false, false, 0);
        PackStart(_thresholdInput, false, false, 0);
        PackStart(_directionSelector, false, false, 0);
        PackStart(_profileInput, true, true, 0);
        PackStart(_removeButton, false, false, 0);
    }

    public string Sensor => _sensorSelector.ActiveText ?? "";
    public double Threshold => _thresholdInput.Value;
    public bool GreaterThan => _directionSelector.ActiveText == Translation.Get("trigger.direction.greater");
    public string TargetProfile => _profileInput.Text;
}
Weiter
