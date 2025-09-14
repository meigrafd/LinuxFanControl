using System;
using System.IO;
using System.Runtime.InteropServices;

namespace FanControl.Gui.Services;

public class ShmReader
{
    private const string ShmPath = "/dev/shm/fancontrol_telemetry";
    private const int SensorCount = 64;
    private const int FloatSize = 4;

    private IntPtr _mappedPtr = IntPtr.Zero;
    private bool _available = false;

    public ShmReader()
    {
        int fd = open(ShmPath, 0);
        if (fd < 0)
        {
            _available = false;
            return;
        }

        _mappedPtr = mmap(IntPtr.Zero, SensorCount * FloatSize, 1, 1, fd, IntPtr.Zero);
        if (_mappedPtr == new IntPtr(-1))
        {
            _available = false;
            return;
        }

        _available = true;
    }

    public float[] ReadValues()
    {
        float[] values = new float[SensorCount];
        if (!_available)
            return values;

        for (int i = 0; i < SensorCount; i++)
        {
            IntPtr offset = IntPtr.Add(_mappedPtr, i * FloatSize);
            values[i] = Marshal.PtrToStructure<float>(offset);
        }

        return values;
    }

    [DllImport("libc", SetLastError = true)]
    private static extern int open(string pathname, int flags);

    [DllImport("libc", SetLastError = true)]
    private static extern IntPtr mmap(IntPtr addr, int length, int prot, int flags, int fd, IntPtr offset);
}
