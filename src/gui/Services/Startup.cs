using Gtk;

public static class Startup
{
    public static void Launch(JsonRpcClient rpc)
    {
        var app = new Application("de.linuxfancontrol.gui", GLib.ApplicationFlags.None);
        app.Register();

        var win = new MainWindow(rpc);
        win.SetDefaultSize(960, 640);
        win.Present();
    }
}
