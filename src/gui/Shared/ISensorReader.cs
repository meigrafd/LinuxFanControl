namespace FanControl.Gui.Shared;

public interface ISensorReader
{
    (double temperature, double pwm) Read();
}
