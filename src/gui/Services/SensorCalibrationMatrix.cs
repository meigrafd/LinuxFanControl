using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace FanControl.Gui.Tools;

public class SensorCalibrationMatrix : VBox
{
    private readonly RpcClient _rpc;

    private TreeView _sensorList;
    private ListStore _sensorStore;
    private Button _applyButton;
    private Label _statusLabel;

    public SensorCalibrationMatrix()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadSensors();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _sensorStore = new ListStore(typeof(string), typeof(double));
        _sensorList = new TreeView(_sensorStore);

        var colLabel = new TreeViewColumn { Title = Translation.Get("calibration.sensor") };
        colLabel.PackStart(new CellRendererText(), true);
        colLabel.AddAttribute(colLabel.CellRenderers[0], "text", 0);
        _sensorList.AppendColumn(colLabel);

        var colOffset = new TreeViewColumn { Title = Translation.Get("calibration.offset") };
        var offsetRenderer = new CellRendererText { Editable = true };
        offsetRenderer.Edited += (o, args) =>
        {
            if (_sensorStore.GetIterFromString(args.Path, out var iter) &&
                double.TryParse(args.NewText, out var val))
            {
                _sensorStore.SetValue(iter, 1, val);
            }
        };
        colOffset.PackStart(offsetRenderer, true);
        colOffset.AddAttribute(offsetRenderer, "text", 1);
        _sensorList.AppendColumn(colOffset);

        var scroll = new ScrolledWindow();
        scroll.Add(_sensorList);
        scroll.SetSizeRequest(400, 200);
        PackStart(scroll, true, true, 0);

        _applyButton = new Button(Translation.Get("calibration.apply"));
        _applyButton.Clicked += (_, _) => ApplyCalibration();
        PackStart(_applyButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadSensors()
    {
        _sensorStore.Clear();

        var response = _rpc.SendRequest("listSensors");
        if (response is JsonObject obj && obj["result"] is JsonArray sensors)
        {
            foreach (var s in sensors)
            {
                var label = s?["label"]?.ToString() ?? "?";
                var path = s?["path"]?.ToString() ?? "";
                _sensorStore.AppendValues($"{label} ({path})", 0.0);
            }
        }
    }

    private void ApplyCalibration()
    {
        var updates = new JsonArray();

        _sensorStore.Foreach((model, path, iter) =>
        {
            string full = (string)model.GetValue(iter, 0);
            double offset = (double)model.GetValue(iter, 1);
            var pathOnly = full.Split('(').LastOrDefault()?.TrimEnd(')');
            if (!string.IsNullOrEmpty(pathOnly))
            {
                updates.Add(new JsonObject
                {
                    ["path"] = pathOnly,
                    ["offset"] = offset
                });
            }
            return false;
        });

        var payload = new JsonObject { ["updates"] = updates };
        var response = _rpc.SendRequest("applySensorOffsets", payload);
        if (response is JsonObject obj && obj["result"]?.ToString() == "ok")
        {
            _statusLabel.Text = Translation.Get("calibration.success");
        }
        else
        {
            _statusLabel.Text = Translation.Get("calibration.error");
        }
    }

    private void Redraw()
    {
        _applyButton.Label = Translation.Get("calibration.apply");
    }
}
