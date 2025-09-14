using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace FanControl.Gui.Tools;

public class SensorFusionEngine : VBox
{
    private readonly RpcClient _rpc;

    private Entry _virtualLabelEntry;
    private ComboBoxText _methodSelector;
    private TreeView _sensorList;
    private ListStore _sensorStore;
    private Button _createButton;
    private Label _statusLabel;

    public SensorFusionEngine()
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

        _virtualLabelEntry = new Entry { PlaceholderText = Translation.Get("fusion.label") };
        PackStart(_virtualLabelEntry, false, false, 0);

        _methodSelector = new ComboBoxText();
        _methodSelector.AppendText("average");
        _methodSelector.AppendText("maximum");
        _methodSelector.AppendText("minimum");
        _methodSelector.Active = 0;
        PackStart(_methodSelector, false, false, 0);

        _sensorStore = new ListStore(typeof(bool), typeof(string));
        _sensorList = new TreeView(_sensorStore);

        var toggleRenderer = new CellRendererToggle { Activatable = true };
        toggleRenderer.Toggled += (o, args) =>
        {
            if (_sensorStore.GetIterFromString(args.Path, out var iter))
            {
                bool current = (bool)_sensorStore.GetValue(iter, 0);
                _sensorStore.SetValue(iter, 0, !current);
            }
        };

        var colToggle = new TreeViewColumn { Title = "" };
        colToggle.PackStart(toggleRenderer, true);
        colToggle.AddAttribute(toggleRenderer, "active", 0);
        _sensorList.AppendColumn(colToggle);

        var colLabel = new TreeViewColumn { Title = "Sensor" };
        colLabel.PackStart(new CellRendererText(), true);
        colLabel.AddAttribute(colLabel.CellRenderers[0], "text", 1);
        _sensorList.AppendColumn(colLabel);

        var scroll = new ScrolledWindow();
        scroll.Add(_sensorList);
        scroll.SetSizeRequest(300, 150);
        PackStart(scroll, true, true, 0);

        _createButton = new Button(Translation.Get("fusion.create"));
        _createButton.Clicked += (_, _) => CreateFusion();
        PackStart(_createButton, false, false, 0);

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
                _sensorStore.AppendValues(false, $"{label} ({path})");
            }
        }
    }

    private void CreateFusion()
    {
        var label = _virtualLabelEntry.Text;
        var method = _methodSelector.ActiveText;
        var sources = new JsonArray();

        _sensorStore.Foreach((model, path, iter) =>
        {
            bool selected = (bool)model.GetValue(iter, 0);
            string full = (string)model.GetValue(iter, 1);
            if (selected)
            {
                var pathOnly = full.Split('(').LastOrDefault()?.TrimEnd(')');
                if (!string.IsNullOrEmpty(pathOnly))
                    sources.Add(pathOnly);
            }
            return false;
        });

        if (!string.IsNullOrEmpty(label) && sources.Count > 0)
        {
            var payload = new JsonObject
            {
                ["label"] = label,
                ["method"] = method,
                ["sources"] = sources
            };
            _rpc.SendRequest("createVirtualSensor", payload);
            _statusLabel.Text = Translation.Get("fusion.success");
        }
        else
        {
            _statusLabel.Text = Translation.Get("fusion.invalid");
        }
    }

    private void Redraw()
    {
        _createButton.Label = Translation.Get("fusion.create");
    }
}
