using Gtk;
using FanControl.Gui.Profile;
using System;

namespace FanControl.Gui.Widgets;

public class ProfileSelector : Box
{
    private readonly ComboBoxText _comboBox;
    private readonly Button _applyButton;
    private readonly ProfileBinder _binder;

    public ProfileSelector(ProfileBinder binder) : base(Orientation.Horizontal, 10)
    {
        _binder = binder;

        _comboBox = new ComboBoxText();
        foreach (var name in _binder.ListProfiles())
            _comboBox.AppendText(name);

        _comboBox.Active = 0;

        _applyButton = new Button("Apply");
        _applyButton.Clicked += (_, _) => ApplyProfile();

        PackStart(_comboBox, false, false, 0);
        PackStart(_applyButton, false, false, 0);
    }

    private void ApplyProfile()
    {
        string selected = _comboBox.ActiveText ?? "default";
        _binder.SetActive(selected);
    }
}
