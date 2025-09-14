using Gtk;
using System;
using FanControl.Gui.Views;

namespace FanControl.Gui.Widgets;

public class CurveEditorToolbar : HBox
{
    public event EventHandler? SaveClicked;
    public event EventHandler? ResetClicked;
    public event EventHandler? ClearClicked;

    public CurveEditorToolbar()
    {
        Spacing = 10;

        var saveButton = new Button(Translation.Get("curve.save"));
        saveButton.Clicked += (_, _) => SaveClicked?.Invoke(this, EventArgs.Empty);
        PackStart(saveButton, false, false, 0);

        var resetButton = new Button(Translation.Get("curve.reset"));
        resetButton.Clicked += (_, _) => ResetClicked?.Invoke(this, EventArgs.Empty);
        PackStart(resetButton, false, false, 0);

        var clearButton = new Button(Translation.Get("curve.clear"));
        clearButton.Clicked += (_, _) => ClearClicked?.Invoke(this, EventArgs.Empty);
        PackStart(clearButton, false, false, 0);
    }
}
