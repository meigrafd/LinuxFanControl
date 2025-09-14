using Gtk;
using FanControl.Gui.Views;

namespace FanControl.Gui.Views;

public class MainWindow : Window
{
    private MainView _mainView;

    public MainWindow() : base("FanControl")
    {
        SetDefaultSize(800, 600);
        SetPosition(WindowPosition.Center);

        _mainView = new MainView();
        Add(_mainView);

        DeleteEvent += (_, _) => Application.Quit();
        ShowAll();
    }
}
