using Gtk;
using System;

namespace FanControl.Gui;

public static class Program
{
    public static void Main(string[] args)
    {
        Application.Init();

        var window = new MainWindow();
        window.Show();

        Application.Run();
    }
}
