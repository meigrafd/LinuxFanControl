using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;
using System.Collections.Generic;

namespace FanControl.Gui.Tools;

public class FanControlProfileDesigner : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _channelSelector;
    private TreeView _pointList;
    private ListStore _pointStore;
    private Button _addButton;
    private Button _saveButton;
    private Label _statusLabel;

    public FanControlProfileDesigner()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadChannels();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _channelSelector = new ComboBoxText();
        _channelSelector.Changed += (_, _) => LoadPoints();
        PackStart(_channelSelector, false, false, 0);

        _pointStore = new ListStore(typeof(double), typeof(double));
        _pointList = new TreeView(_pointStore);

        var colX = new TreeViewColumn { Title = "Sensor °C" };
        colX.PackStart(new CellRendererText(), true);
        colX.AddAttribute(colX.CellRenderers[0], "text", 0);
        _pointList.AppendColumn(colX);

        var colY = new TreeViewColumn { Title = "Fan %" };
        colY.PackStart(new CellRendererText(), true);
        colY.AddAttribute(colY.CellRenderers[0], "text", 1);
        _pointList.AppendColumn(colY);

        var scroll = new ScrolledWindow();
        scroll.Add(_pointList);
        scroll.SetSizeRequest(300, 150);
        PackStart(scroll, true, true, 0);

        _addButton = new Button(Translation.Get("profile.addpoint"));
        _addButton.Clicked += (_, _) => AddPoint();
        PackStart(_addButton, false, false, 0);

        _saveButton = new Button(Translation.Get("profile.save"));
        _saveButton.Clicked += (_, _) => SaveProfile();
        PackStart(_saveButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadChannels()
    {
        _channelSelector.RemoveAll();

        var response = _rpc.SendRequest("listChannels");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            foreach (var ch in channels)
            {
                var name = ch?["name"]?.ToString() ?? "";
                _channelSelector.AppendText(name);
            }

            if (_channelSelector.Children.Length > 0)
                _channelSelector.Active = 0;
        }
    }

    private void LoadPoints()
    {
        _pointStore.Clear();

        var selected = _channelSelector.ActiveText;
        if (string.IsNullOrEmpty(selected)) return;

        var response = _rpc.SendRequest("listChannels");
        if (response is JsonObject obj && obj["result"] is JsonArray channels)
        {
            foreach (var ch in channels)
            {
                if (ch?["name"]?.ToString() == selected && ch["points"] is JsonArray pts)
                {
                    foreach (var pt in pts)
                    {
                        double x = pt?["x"]?.GetValue<double>() ?? 0.0;
                        double y = pt?["y"]?.GetValue<double>() ?? 0.0;
                        _pointStore.AppendValues(x, y);
                    }
                    break;
                }
            }
        }
    }

    private void AddPoint()
    {
        _pointStore.AppendValues(50.0, 50.0);
    }

    private void SaveProfile()
    {
        var selected = _channelSelector.ActiveText;
        if (string.IsNullOrEmpty(selected)) return;

        var points = new JsonArray();
        _pointStore.Foreach((model, path, iter) =>
        {
            double x = (double)model.GetValue(iter, 0);
            double y = (double)model.GetValue(iter, 1);
            points.Add(new JsonObject { ["x"] = x, ["y"] = y });
            return false;
        });

        var payload = new JsonObject
        {
            ["name"] = selected,
            ["points"] = points
        };
        _rpc.SendRequest("updateChannelCurve", payload);
        _statusLabel.Text = Translation.Get("profile.saved");
    }

    private void Redraw()
    {
        _addButton.Label = Translation.Get("profile.addpoint");
        _saveButton.Label = Translation.Get("profile.save");
    }
}
