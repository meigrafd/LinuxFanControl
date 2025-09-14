using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class SensorAccessAuditor : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _sensorSelector;
    private Button _auditButton;
    private TreeView _accessView;
    private ListStore _accessStore;

    public SensorAccessAuditor()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadSensors();
    }

    private void BuildUi()
    {
        _sensorSelector = new ComboBoxText();
        PackStart(_sensorSelector, false, false, 0);

        _auditButton = new Button(Translation.Get("audit.run"));
        _auditButton.Clicked += (_, _) => RunAudit();
        PackStart(_auditButton, false, false, 0);

        _accessStore = new ListStore(typeof(string), typeof(string));
        _accessView = new TreeView(_accessStore);

        var colComponent = new TreeViewColumn { Title = Translation.Get("audit.component") };
        colComponent.PackStart(new CellRendererText(), true);
        colComponent.AddAttribute(colComponent.CellRenderers[0], "text", 0);
        _accessView.AppendColumn(colComponent);

        var colMode = new TreeViewColumn { Title = Translation.Get("audit.mode") };
        colMode.PackStart(new CellRendererText(), true);
        colMode.AddAttribute(colMode.CellRenderers[0], "text", 1);
        _accessView.AppendColumn(colMode);

        var scroll = new ScrolledWindow();
        scroll.Add(_accessView);
        scroll.SetSizeRequest(500, 200);
        PackStart(scroll, true, true, 0);
    }

    private void LoadSensors()
    {
        _sensorSelector.RemoveAll();

        var response = _rpc.SendRequest("listSensors");
        if (response is JsonObject obj && obj["result"] is JsonArray sensors)
        {
            foreach (var s in sensors)
            {
                var label = s?["label"]?.ToString() ?? "?";
                var path = s?["path"]?.ToString() ?? "";
                _sensorSelector.AppendText($"{label} ({path})");
            }

            if (_sensorSelector.Children.Length > 0)
                _sensorSelector.Active = 0;
        }
    }

    private void RunAudit()
    {
        _accessStore.Clear();

        var sensor = _sensorSelector.ActiveText?.Split('(').LastOrDefault()?.TrimEnd(')');
        if (!string.IsNullOrEmpty(sensor))
        {
            var payload = new JsonObject { ["path"] = sensor };
            var response = _rpc.SendRequest("auditSensorAccess", payload);
            if (response is JsonObject obj && obj["result"] is JsonArray accesses)
            {
                foreach (var a in accesses)
                {
                    var component = a?["component"]?.ToString() ?? "?";
                    var mode = a?["mode"]?.ToString() ?? "?";
                    _accessStore.AppendValues(component, mode);
                }
            }
        }
    }

    private void Redraw()
    {
        _auditButton.Label = Translation.Get("audit.run");
    }
}
