using Gtk;
using FanControl.Gui.Views;

namespace FanControl.Gui;

public class MainWindow : Window
{
    private readonly MainView _mainView;

    public MainWindow() : base("Fan Control")
    {
        SetDefaultSize(1000, 700);
        SetPosition(WindowPosition.Center);
        BorderWidth = 10;

        _mainView = new MainView();
        Add(_mainView);

        DeleteEvent += (_, _) =>
        {
            Application.Quit();
        };

        ShowAll();
    }
}
