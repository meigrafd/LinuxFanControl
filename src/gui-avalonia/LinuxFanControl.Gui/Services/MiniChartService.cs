#nullable enable
using System;
using System.Collections.Generic;

namespace LinuxFanControl.Gui.Services
{
    public sealed class MiniChartBuffer
    {
        private readonly int _capacity;
        private readonly Queue<double> _q = new();

        public MiniChartBuffer(int capacity = 60) { _capacity = Math.Max(10, capacity); }

        public void Push(double v)
        {
            _q.Enqueue(v);
            while (_q.Count > _capacity) _q.Dequeue();
        }

        public double[] Snapshot()
        {
            return _q.ToArray();
        }
    }
}
