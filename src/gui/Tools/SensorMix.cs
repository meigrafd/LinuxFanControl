public class SensorMix
{
    private readonly Dictionary<string, float> _weights = new();

    public void AddSensor(string id, float weight)
    {
        _weights[id] = weight;
    }

    public void RemoveSensor(string id)
    {
        _weights.Remove(id);
    }

    public float Evaluate(Dictionary<string, float> sensorValues)
    {
        float totalWeight = 0;
        float weightedSum = 0;

        foreach (var (id, weight) in _weights)
        {
            if (sensorValues.TryGetValue(id, out var value))
            {
                weightedSum += value * weight;
                totalWeight += weight;
            }
        }

        return totalWeight > 0 ? weightedSum / totalWeight : 0;
    }

    public IReadOnlyDictionary<string, float> Weights => _weights;
}
