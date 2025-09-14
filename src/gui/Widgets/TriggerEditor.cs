using Gtk;
using System;
using FanControl.Gui.Views;

namespace FanControl.Gui.Widgets;

public class TriggerEditor : VBox
{
    private readonly ListBox _triggerList;
    private readonly Button _addButton;

    public TriggerEditor()
    {
        Spacing = 10;

        Label title = new(Translation.Get("trigger.title"));
        title.SetMarkup($"<b>{Translation.Get("trigger.title")}</b>");
        PackStart(title, false, false, 0);

        _triggerList = new ListBox();
        PackStart(_triggerList, true, true, 0);

        _addButton = new Button(Translation.Get("trigger.add"));
        _addButton.Clicked += OnAddTrigger;
        PackStart(_addButton, false, false, 0);
    }

    private void OnAddTrigger(object? sender, EventArgs e)
    {
        var row = new TriggerRow();
        _triggerList.Add(row);
        _triggerList.ShowAll();
    }
}
