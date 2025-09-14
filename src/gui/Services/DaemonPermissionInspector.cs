using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class DaemonPermissionInspector : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _componentSelector;
    private Button _inspectButton;
    private TreeView _permissionView;
    private ListStore _permissionStore;

    public DaemonPermissionInspector()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadComponents();
    }

    private void BuildUi()
    {
        _componentSelector = new ComboBoxText();
        PackStart(_componentSelector, false, false, 0);

        _inspectButton = new Button(Translation.Get("perm.inspect"));
        _inspectButton.Clicked += (_, _) => InspectPermissions();
        PackStart(_inspectButton, false, false, 0);

        _permissionStore = new ListStore(typeof(string), typeof(string));
        _permissionView = new TreeView(_permissionStore);

        var colResource = new TreeViewColumn { Title = Translation.Get("perm.resource") };
        colResource.PackStart(new CellRendererText(), true);
        colResource.AddAttribute(colResource.CellRenderers[0], "text", 0);
        _permissionView.AppendColumn(colResource);

        var colAccess = new TreeViewColumn { Title = Translation.Get("perm.access") };
        colAccess.PackStart(new CellRendererText(), true);
        colAccess.AddAttribute(colAccess.CellRenderers[0], "text", 1);
        _permissionView.AppendColumn(colAccess);

        var scroll = new ScrolledWindow();
        scroll.Add(_permissionView);
        scroll.SetSizeRequest(500, 200);
        PackStart(scroll, true, true, 0);
    }

    private void LoadComponents()
    {
        _componentSelector.RemoveAll();

        var response = _rpc.SendRequest("listDaemonComponents");
        if (response is JsonObject obj && obj["result"] is JsonArray components)
        {
            foreach (var c in components)
            {
                var name = c?.ToString() ?? "?";
                _componentSelector.AppendText(name);
            }

            if (_componentSelector.Children.Length > 0)
                _componentSelector.Active = 0;
        }
    }

    private void InspectPermissions()
    {
        _permissionStore.Clear();

        var component = _componentSelector.ActiveText;
        if (!string.IsNullOrEmpty(component))
        {
            var payload = new JsonObject { ["name"] = component };
            var response = _rpc.SendRequest("inspectComponentPermissions", payload);
            if (response is JsonObject obj && obj["result"] is JsonArray perms)
            {
                foreach (var p in perms)
                {
                    var res = p?["resource"]?.ToString() ?? "?";
                    var mode = p?["access"]?.ToString() ?? "?";
                    _permissionStore.AppendValues(res, mode);
                }
            }
        }
    }

    private void Redraw()
    {
        _inspectButton.Label = Translation.Get("perm.inspect");
    }
}
