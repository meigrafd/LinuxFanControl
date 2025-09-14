using Gtk;
using FanControl.Gui;
using System;

namespace FanControl.Gui.Widgets;

public class LocaleSwitcher : Box
{
    private readonly ComboBoxText _comboBox;
    private readonly Button _applyButton;

    public LocaleSwitcher() : base(Orientation.Horizontal, 10)
    {
        _comboBox = new ComboBoxText();
        _comboBox.AppendText("en");
        _comboBox.AppendText("de");
        _comboBox.Active = 0;

        _applyButton = new Button("Apply");
        _applyButton.Clicked += (_, _) => ApplyLocale();

        PackStart(_comboBox, false, false, 0);
        PackStart(_applyButton, false, false, 0);
    }

    private void ApplyLocale()
    {
        string selected = _comboBox.ActiveText ?? "en";
        Translation.Load(selected);
    }
}
