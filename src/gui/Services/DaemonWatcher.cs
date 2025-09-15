using System;
using System.Threading.Tasks;

public class DaemonWatcher
{
    private readonly DaemonPing _ping;
    private readonly DaemonState _state;

    public event Action<DaemonState>? OnStatusChanged;

    public DaemonWatcher(DaemonPing ping, DaemonState state)
    {
        _ping = ping;
        _state = state;
    }

    public void Start()
    {
        Task.Run(async () =>
        {
            while (true)
            {
                var alive = _ping.IsAlive();
                var changed = alive != _state.IsRunning;

                _state.IsRunning = alive;
                _state.LastUpdate = DateTime.Now;

                if (changed)
                    OnStatusChanged?.Invoke(_state);

                await Task.Delay(1000);
            }
        });
    }
}
