using System;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;

public class ShmReader
{
    private const string ShmName = "/linuxfancontrol.telemetry";
    private const int BufferSize = 65536;

    private MemoryMappedFile? _mmf;
    private MemoryMappedViewAccessor? _accessor;

    public bool Init()
    {
        try
        {
            _mmf = MemoryMappedFile.OpenExisting(ShmName, MemoryMappedFileRights.Read);
            _accessor = _mmf.CreateViewAccessor(0, BufferSize, MemoryMappedFileAccess.Read);
            return true;
        }
        catch
        {
            return false;
        }
    }

    public byte[]? ReadRaw()
    {
        if (_accessor == null)
            return null;

        var buffer = new byte[BufferSize];
        _accessor.ReadArray(0, buffer, 0, BufferSize);
        return buffer;
    }

    public Span<byte> ReadSpan()
    {
        if (_accessor == null)
            return Span<byte>.Empty;

        unsafe
        {
            byte* ptr = null;
            _accessor.SafeMemoryMappedViewHandle.AcquirePointer(ref ptr);
            return new Span<byte>(ptr, BufferSize);
        }
    }

    public void Dispose()
    {
        _accessor?.Dispose();
        _mmf?.Dispose();
    }
}
