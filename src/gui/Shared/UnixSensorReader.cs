using System.Net.Sockets;
using System.Text;

namespace FanControl.Gui.Shared;

public class UnixSensorReader
{
    public (double temperature, double pwm) Read()
    {
        using var socket = new Socket(AddressFamily.Unix, SocketType.Stream, ProtocolType.Unspecified);
        socket.Connect(new UnixDomainSocketEndPoint("/tmp/fancontrol.sock"));

        var buffer = new byte[128];
        int bytes = socket.Receive(buffer);
        var line = Encoding.UTF8.GetString(buffer, 0, bytes);

        if (string.IsNullOrWhiteSpace(line))
            return (0, 0);

        var parts = line.Split(',');
        if (parts.Length < 2)
            return (0, 0);

        double temp = double.TryParse(parts[0], out var t) ? t : 0;
        double pwm = double.TryParse(parts[1], out var p) ? p : 0;

        return (temp, pwm);
    }
}
