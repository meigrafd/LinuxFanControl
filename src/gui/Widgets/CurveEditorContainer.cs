using Gtk;
using System;
using FanControl.Gui.Views;

namespace FanControl.Gui.Widgets;

public class CurveEditorContainer : VBox
{
    private readonly CurveEditor _editor;
    private readonly CurveEditorToolbar _toolbar;

    public CurveEditorContainer()
    {
        Spacing = 10;

        Label title = new(Translation.Get("curve.title"));
        title.SetMarkup($"<b>{Translation.Get("curve.title")}</b>");
        PackStart(title, false, false, 0);

        _toolbar = new CurveEditorToolbar();
        _toolbar.SaveClicked += OnSave;
        _toolbar.ResetClicked += OnReset;
        _toolbar.ClearClicked += OnClear;
        PackStart(_toolbar, false, false, 0);

        _editor = new CurveEditor();
        PackStart(_editor, true, true, 0);
    }

    private void OnSave(object? sender, EventArgs e)
    {
        var points = _editor.GetPoints();
        // TODO: Save logic
    }

    private void OnReset(object? sender, EventArgs e)
    {
        _editor.ResetPoints();
    }

    private void OnClear(object? sender, EventArgs e)
    {
        _editor.ClearPoints();
    }
}
