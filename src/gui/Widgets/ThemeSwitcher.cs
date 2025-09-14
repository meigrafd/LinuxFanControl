using Gtk;
using FanControl.Gui.Views;

namespace FanControl.Gui.Widgets;

public class ThemeSwitcher : Box
{
    private readonly ComboBoxText _comboBox;
    private readonly Button _applyButton;

    public ThemeSwitcher() : base(Orientation.Horizontal, 10)
    {
        _comboBox = new ComboBoxText();
        _comboBox.AppendText("light");
        _comboBox.AppendText("dark");
        _comboBox.Active = 0;

        _applyButton = new Button("Apply");
        _applyButton.Clicked += (_, _) => ApplyTheme();

        PackStart(_comboBox, false, false, 0);
        PackStart(_applyButton, false, false, 0);
    }

    private void ApplyTheme()
    {
        string selected = _comboBox.ActiveText ?? "light";
        ThemeManager.Apply(selected);
    }
}
