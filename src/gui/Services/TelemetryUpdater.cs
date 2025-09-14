using System.Timers;
using FanControl.Gui.Shared;

namespace FanControl.Gui.Views;

public class TelemetryUpdater
{
    private readonly Timer _timer;
    private readonly UnixSensorReader _reader;

    public delegate void TelemetryUpdatedHandler(double temperature, double pwm);
    public event TelemetryUpdatedHandler? Updated;

    public TelemetryUpdater()
    {
        _reader = new UnixSensorReader();

        _timer = new Timer(1000);
        _timer.Elapsed += (_, _) => Update();
        _timer.Start();
    }

    private void Update()
    {
        try
        {
            var (temperature, pwm) = _reader.Read();
            Updated?.Invoke(temperature, pwm);
        }
        catch
        {
            // Fehlerbehandlung optional
        }
    }
}
