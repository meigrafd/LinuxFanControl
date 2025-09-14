using Gtk;

namespace FanControl.Gui.Widgets;

public class SensorTile : Box
{
    private readonly Label _temperatureLabel;
    private readonly Label _pwmLabel;

    public SensorTile() : base(Orientation.Vertical, 10)
    {
        Margin = 20;

        _temperatureLabel = new Label("Temperature: -- °C");
        _pwmLabel = new Label("Fan PWM: -- %");

        PackStart(new Label("Sensor Data"), false, false, 0);
        PackStart(_temperatureLabel, false, false, 0);
        PackStart(_pwmLabel, false, false, 0);
    }

    public void UpdateValue(double temperature, double pwm)
    {
        _temperatureLabel.Text = $"Temperature: {temperature:F1} °C";
        _pwmLabel.Text = $"Fan PWM: {pwm:F0} %";
    }
}
