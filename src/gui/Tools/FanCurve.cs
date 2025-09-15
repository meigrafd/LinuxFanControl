public class FanCurve
{
    private readonly SortedDictionary<float, float> _points = new();

    public void AddPoint(float temperature, float percent)
    {
        _points[temperature] = percent;
    }

    public void RemovePoint(float temperature)
    {
        _points.Remove(temperature);
    }

    public float Evaluate(float temperature)
    {
        if (_points.Count == 0) return 0;

        var keys = _points.Keys.ToList();

        if (temperature <= keys[0])
            return _points[keys[0]];

        if (temperature >= keys[^1])
            return _points[keys[^1]];

        for (int i = 0; i < keys.Count - 1; i++)
        {
            var t1 = keys[i];
            var t2 = keys[i + 1];

            if (temperature >= t1 && temperature <= t2)
            {
                var p1 = _points[t1];
                var p2 = _points[t2];
                var ratio = (temperature - t1) / (t2 - t1);
                return p1 + ratio * (p2 - p1);
            }
        }

        return 0;
    }

    public IReadOnlyDictionary<float, float> Points => _points;
}
