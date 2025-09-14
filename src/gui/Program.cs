using Gtk;
using FanControl.Gui;

namespace FanControl;

public static class Program
{
    public static void Main(string[] args)
    {
        Application.Init();

        Translation.Load("en");

        var window = new MainWindow();
        window.Show();

        Application.Run();
    }
}
