using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading;
using Avalonia.Threading;

namespace LinuxFanControl.Gui.Services
{
    // Mock telemetry: generates N fan channels with RPM/Temp and allows duty change
    public class FanModel
    {
        public string Name { get; set; }
        public double Rpm { get; set; }
        public double Duty { get; set; }
        public double Temp { get; set; }
        public FanModel(string name) { Name = name; }
    }

    public sealed class MockTelemetryService : IDisposable
    {
        private readonly Timer _timer;
        private readonly Random _rng = new Random();
        public ObservableCollection<FanModel> Fans { get; } = new();

        public MockTelemetryService(int count = 6)
        {
            for (int i = 1; i <= count; i++)
                Fans.Add(new FanModel($"Fan {i}") { Duty = 40 + (i%3)*10 });

            _timer = new Timer(_ => Tick(), null, 500, 500);
        }

        private void Tick()
        {
            Dispatcher.UIThread.Post(() =>
            {
                foreach (var f in Fans)
                {
                    var baseRpm = 600 + f.Duty * 20;
                    var jitter = _rng.NextDouble() * 50.0 - 25.0;
                    f.Rpm = Math.Max(0, baseRpm + jitter);
                    f.Temp = 25.0 + 0.6 * f.Duty + (_rng.NextDouble() * 2 - 1);
                }
            });
        }

        public void SetDuty(FanModel fan, double duty)
        {
            fan.Duty = Math.Clamp(duty, 0, 100);
        }

        public void Dispose() => _timer.Dispose();
    }
}
