using System;
using System.Collections.ObjectModel;

namespace LinuxFanControl.Gui.Models
{
    public class Sensor
    {
        public string Id { get; set; } = Guid.NewGuid().ToString();
        public string Label { get; set; } = "";
        public string Type { get; set; } = "Unknown";
        public double Value { get; set; } = 0.0;
        public string Unit { get; set; } = "°C";
    }

    public class Fan
    {
        public string Id { get; set; } = Guid.NewGuid().ToString();
        public string Label { get; set; } = "";
        public int Rpm { get; set; } = 0;
        public double Duty { get; set; } = 0.0;
    }

    public class Channel
    {
        public string Id { get; set; } = Guid.NewGuid().ToString();
        public string Label { get; set; } = "";
        public string Mode { get; set; } = "Auto";
        public string SensorLabel { get; set; } = "";
        public double Output { get; set; } = 0.0;
    }

    public class DetectionResult
    {
        public Sensor[] Sensors { get; set; } = Array.Empty<Sensor>();
        public Fan[] Fans { get; set; } = Array.Empty<Fan>();
    }
}
