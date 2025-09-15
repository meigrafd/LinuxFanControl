public class SensorManager
{
    private readonly Dictionary<string, float> _values = new();

    public void Update(string id, float value)
    {
        _values[id] = value;
    }

    public float Get(string id)
    {
        return _values.TryGetValue(id, out var v) ? v : 0;
    }

    public IReadOnlyDictionary<string, float> All => _values;
}
