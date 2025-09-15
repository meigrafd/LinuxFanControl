public class DaemonState
{
    public bool IsRunning { get; set; }
    public bool HasError { get; set; }
    public DateTime LastUpdate { get; set; }

    public DaemonState()
    {
        IsRunning = false;
        HasError = false;
        LastUpdate = DateTime.MinValue;
    }

    public string GetStatusText()
    {
        if (HasError)
            return LocaleManager._("daemon.status.error");

        return IsRunning
        ? LocaleManager._("daemon.status.running")
        : LocaleManager._("daemon.status.stopped");
    }
}
