using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlLoadBalancer : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _groupSelector;
    private ComboBoxText _strategySelector;
    private Button _applyButton;
    private Label _statusLabel;

    public FanControlLoadBalancer()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadGroups();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _groupSelector = new ComboBoxText();
        PackStart(_groupSelector, false, false, 0);

        _strategySelector = new ComboBoxText();
        _strategySelector.AppendText("round-robin");
        _strategySelector.AppendText("priority");
        _strategySelector.AppendText("adaptive");
        _strategySelector.Active = 0;
        PackStart(_strategySelector, false, false, 0);

        _applyButton = new Button(Translation.Get("loadbalance.apply"));
        _applyButton.Clicked += (_, _) => ApplyStrategy();
        PackStart(_applyButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadGroups()
    {
        _groupSelector.RemoveAll();

        var response = _rpc.SendRequest("listFanGroups");
        if (response is JsonObject obj && obj["result"] is JsonArray groups)
        {
            foreach (var g in groups)
            {
                var name = g?.ToString() ?? "?";
                _groupSelector.AppendText(name);
            }

            if (_groupSelector.Children.Length > 0)
                _groupSelector.Active = 0;
        }
    }

    private void ApplyStrategy()
    {
        var group = _groupSelector.ActiveText;
        var strategy = _strategySelector.ActiveText;

        if (!string.IsNullOrEmpty(group) && !string.IsNullOrEmpty(strategy))
        {
            var payload = new JsonObject
            {
                ["group"] = group,
                ["strategy"] = strategy
            };
            var response = _rpc.SendRequest("setLoadBalancingStrategy", payload);
            if (response is JsonObject obj && obj["result"]?.ToString() == "ok")
            {
                _statusLabel.Text = Translation.Get("loadbalance.success");
            }
            else
            {
                _statusLabel.Text = Translation.Get("loadbalance.error");
            }
        }
        else
        {
            _statusLabel.Text = Translation.Get("loadbalance.invalid");
        }
    }

    private void Redraw()
    {
        _applyButton.Label = Translation.Get("loadbalance.apply");
    }
}
