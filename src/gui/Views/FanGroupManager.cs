using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Views;

public class FanGroupManager : VBox
{
    private readonly RpcClient _rpc;

    private Entry _groupNameEntry;
    private ComboBoxText _fanSelector;
    private Button _addButton;
    private Label _statusLabel;

    public FanGroupManager()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadFans();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _groupNameEntry = new Entry { PlaceholderText = Translation.Get("group.name") };
        PackStart(_groupNameEntry, false, false, 0);

        _fanSelector = new ComboBoxText();
        PackStart(_fanSelector, false, false, 0);

        _addButton = new Button(Translation.Get("group.add"));
        _addButton.Clicked += (_, _) => AddToGroup();
        PackStart(_addButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadFans()
    {
        _fanSelector.RemoveAll();

        var response = _rpc.SendRequest("listPwms");
        if (response is JsonObject obj && obj["result"] is JsonArray fans)
        {
            foreach (var fan in fans)
            {
                var label = fan?["label"]?.ToString() ?? "?";
                _fanSelector.AppendText(label);
            }

            if (_fanSelector.Children.Length > 0)
                _fanSelector.Active = 0;
        }
    }

    private void AddToGroup()
    {
        var group = _groupNameEntry.Text;
        var fan = _fanSelector.ActiveText;

        if (!string.IsNullOrEmpty(group) && !string.IsNullOrEmpty(fan))
        {
            var payload = new JsonObject
            {
                ["group"] = group,
                ["label"] = fan
            };
            _rpc.SendRequest("addFanToGroup", payload);
            _statusLabel.Text = Translation.Get("group.success");
        }
        else
        {
            _statusLabel.Text = Translation.Get("group.invalid");
        }
    }

    private void Redraw()
    {
        _addButton.Label = Translation.Get("group.add");
    }
}
