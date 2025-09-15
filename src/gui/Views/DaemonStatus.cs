public static class DaemonStatus
{
    public static string GetStatusText(bool running, bool error)
    {
        if (error)
            return LocaleManager._("daemon.status.error");

        return running
        ? LocaleManager._("daemon.status.running")
        : LocaleManager._("daemon.status.stopped");
    }
}
