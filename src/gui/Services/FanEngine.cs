using System;
using System.Collections.Generic;

namespace FanControl.Gui.Engine;

public class FanEngine
{
    private List<(double x, double y)> _curve = new();
    private double _lastTemperature = 0.0;

    public void SetCurve(List<(double x, double y)> points)
    {
        _curve = new(points);
        _curve.Sort((a, b) => a.x.CompareTo(b.x));
    }

    public void UpdateTemperature(double temperature)
    {
        _lastTemperature = temperature;
    }

    public double GetPwm()
    {
        if (_curve.Count == 0)
            return 0;

        for (int i = 0; i < _curve.Count - 1; i++)
        {
            var (x1, y1) = _curve[i];
            var (x2, y2) = _curve[i + 1];

            if (_lastTemperature >= x1 && _lastTemperature <= x2)
            {
                double t = (_lastTemperature - x1) / (x2 - x1);
                return y1 + t * (y2 - y1);
            }
        }

        if (_lastTemperature < _curve[0].x)
            return _curve[0].y;

        return _curve[^1].y;
    }
}
