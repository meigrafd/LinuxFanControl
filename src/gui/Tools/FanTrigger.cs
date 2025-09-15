public class FanTrigger
{
    public float IdleTemp { get; }
    public float IdlePercent { get; }
    public float LoadTemp { get; }
    public float LoadPercent { get; }
    public float ResponseTime { get; }

    private float _current;

    public FanTrigger(float idleTemp, float idlePercent, float loadTemp, float loadPercent, float responseTime)
    {
        IdleTemp = idleTemp;
        IdlePercent = idlePercent;
        LoadTemp = loadTemp;
        LoadPercent = loadPercent;
        ResponseTime = responseTime;
        _current = idlePercent;
    }

    public float Evaluate(float temperature)
    {
        float target = temperature < IdleTemp ? IdlePercent :
        temperature > LoadTemp ? LoadPercent :
        IdlePercent + (temperature - IdleTemp) / (LoadTemp - IdleTemp) * (LoadPercent - IdlePercent);

        _current += (target - _current) * ResponseTime;
        return _current;
    }
}
