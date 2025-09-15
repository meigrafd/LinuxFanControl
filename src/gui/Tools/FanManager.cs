public class FanManager
{
    private readonly Dictionary<string, FanLogic> _fans = new();

    public void Register(FanLogic logic)
    {
        _fans[logic.FanId] = logic;
    }

    public void Update(Dictionary<string, float> sensorValues, Action<string, float> apply)
    {
        foreach (var fan in _fans.Values)
        {
            var percent = fan.Evaluate(sensorValues);
            apply(fan.FanId, percent);
        }
    }

    public IReadOnlyDictionary<string, FanLogic> Fans => _fans;
}
