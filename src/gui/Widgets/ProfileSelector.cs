using System;
using Gtk;
using System.Text.Json.Nodes;
using FanControl.Gui.Services;

namespace FanControl.Gui.Widgets;

public class ProfileSelector : HBox
{
    private Label _label;
    private ComboBoxText _comboBox;
    private Button _applyButton;
    private readonly RpcClient _rpc;

    /// <summary>
    /// Wird ausgelöst, wenn der Nutzer ein Profil auswählt und anwendet.
    /// </summary>
    public event Action<string>? ProfileSelected;

    public ProfileSelector()
    {
        Spacing = 5;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadProfiles();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _label = new Label(Translation.Get("profile.selector"));
        PackStart(_label, false, false, 0);

        _comboBox = new ComboBoxText();
        PackStart(_comboBox, false, false, 0);

        _applyButton = new Button(Translation.Get("profile.apply"));
        _applyButton.Clicked += (_, _) => OnApply();
        PackStart(_applyButton, false, false, 0);
    }

    private void LoadProfiles()
    {
        _comboBox.RemoveAll();
        try
        {
            var response = _rpc.SendRequest("listChannels");
            if (response is JsonObject obj && obj["result"] is JsonArray channels)
            {
                foreach (var ch in channels)
                {
                    var name = ch?["name"]?.ToString() ?? "";
                    _comboBox.AppendText(name);
                }

                if (_comboBox.Children.Length > 0)
                    _comboBox.Active = 0;
            }
        }
        catch
        {
            // Fehlerbehandlung optional: Anzeige einer Fehlermeldung
        }
    }

    private void OnApply()
    {
        var selected = _comboBox.ActiveText;
        if (!string.IsNullOrEmpty(selected))
            ProfileSelected?.Invoke(selected);
    }

    private void Redraw()
    {
        BuildUi();
        LoadProfiles();
    }
}
