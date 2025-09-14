using Gtk;
using FanControl.Gui.Views;

namespace FanControl.Gui.Views;

public class SettingsWindow : Window
{
    private SettingsView _settingsView;

    public SettingsWindow() : base("Settings")
    {
        SetDefaultSize(400, 300);
        SetPosition(WindowPosition.Center);

        _settingsView = new SettingsView();
        Add(_settingsView);

        DeleteEvent += (_, _) => Hide();
        ShowAll();
    }
}
