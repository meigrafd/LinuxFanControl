using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class SensorGroupEditor : VBox
{
    private readonly RpcClient _rpc;

    private Entry _groupNameEntry;
    private TreeView _sensorList;
    private ListStore _sensorStore;
    private Button _createButton;
    private Label _statusLabel;

    public SensorGroupEditor()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadSensors();
    }

    private void BuildUi()
    {
        _groupNameEntry = new Entry { PlaceholderText = Translation.Get("group.name") };
        PackStart(_groupNameEntry, false, false, 0);

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

        var colLabel = new TreeViewColumn { Title = Translation.Get("group.sensor") };
        colLabel.PackStart(new CellRendererText(), true);
        colLabel.AddAttribute(colLabel.CellRenderers[0], "text", 1);
        _sensorList.AppendColumn(colLabel);

        var scroll = new ScrolledWindow();
        scroll.Add(_sensorList);
        scroll.SetSizeRequest(400, 200);
        PackStart(scroll, true, true, 0);

        _createButton = new Button(Translation.Get("group.create"));
        _createButton.Clicked += (_, _) => CreateGroup();
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

    private void CreateGroup()
    {
        var name = _groupNameEntry.Text;
        var members = new JsonArray();

        _sensorStore.Foreach((model, path, iter) =>
        {
            bool selected = (bool)model.GetValue(iter, 0);
            string full = (string)model.GetValue(iter, 1);
            if (selected)
            {
                var pathOnly = full.Split('(').LastOrDefault()?.TrimEnd(')');
                if (!string.IsNullOrEmpty(pathOnly))
                    members.Add(pathOnly);
            }
            return false;
        });

        if (!string.IsNullOrEmpty(name) && members.Count > 0)
        {
            var payload = new JsonObject
            {
                ["name"] = name,
                ["members"] = members
            };
            var response = _rpc.SendRequest("createSensorGroup", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("group.success")
            : Translation.Get("group.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("group.invalid");
        }
    }

    private void Redraw()
    {
        _createButton.Label = Translation.Get("group.create");
    }
}
