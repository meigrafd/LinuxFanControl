using Gtk;
using System;
using FanControl.Gui.Views;

namespace FanControl.Gui.Widgets;

public class MixEditor : VBox
{
    private readonly ListBox _mixList;
    private readonly Button _addButton;

    public MixEditor()
    {
        Spacing = 10;

        Label title = new(Translation.Get("mix.title"));
        title.SetMarkup($"<b>{Translation.Get("mix.title")}</b>");
        PackStart(title, false, false, 0);

        _mixList = new ListBox();
        PackStart(_mixList, true, true, 0);

        _addButton = new Button(Translation.Get("mix.add"));
        _addButton.Clicked += OnAddMix;
        PackStart(_addButton, false, false, 0);
    }

    private void OnAddMix(object? sender, EventArgs e)
    {
        var row = new MixRow();
        _mixList.Add(row);
        _mixList.ShowAll();
    }
}
