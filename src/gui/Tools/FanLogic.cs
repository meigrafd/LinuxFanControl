public class FanLogic
{
    public string FanId { get; }
    public FanMode Mode { get; set; }

    public float ManualPercent { get; set; } = 0;
    public FanCurve? Curve { get; set; }
    public FanTrigger? Trigger { get; set; }
    public SensorMix Mix { get; } = new();

    public FanLogic(string fanId)
    {
        FanId = fanId;
        Mode = FanMode.Manual;
    }

    public float Evaluate(Dictionary<string, float> sensorValues)
    {
        var input = Mix.Evaluate(sensorValues);

        return Mode switch
        {
            FanMode.Manual => ManualPercent,
            FanMode.Curve => Curve?.Evaluate(input) ?? 0,
            FanMode.Trigger => Trigger?.Evaluate(input) ?? 0,
            _ => 0
        };
    }
}
