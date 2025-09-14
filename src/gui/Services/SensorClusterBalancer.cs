using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class SensorClusterBalancer : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _groupSelector;
    private Button _balanceButton;
    private Label _statusLabel;

    public SensorClusterBalancer()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadGroups();
    }

    private void BuildUi()
    {
        _groupSelector = new ComboBoxText();
        PackStart(_groupSelector, false, false, 0);

        _balanceButton = new Button(Translation.Get("balance.run"));
        _balanceButton.Clicked += (_, _) => BalanceCluster();
        PackStart(_balanceButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void LoadGroups()
    {
        _groupSelector.RemoveAll();

        var response = _rpc.SendRequest("getSensorGroups");
        if (response is JsonObject obj && obj["result"] is JsonArray groups)
        {
            foreach (var g in groups)
            {
                var name = g?["name"]?.ToString() ?? "?";
                _groupSelector.AppendText(name);
            }

            if (_groupSelector.Children.Length > 0)
                _groupSelector.Active = 0;
        }
    }

    private void BalanceCluster()
    {
        var group = _groupSelector.ActiveText;
        if (!string.IsNullOrEmpty(group))
        {
            var payload = new JsonObject { ["group"] = group };
            var response = _rpc.SendRequest("balanceSensorCluster", payload);
            _statusLabel.Text = response is JsonObject obj && obj["result"]?.ToString() == "ok"
            ? Translation.Get("balance.success")
            : Translation.Get("balance.error");
        }
        else
        {
            _statusLabel.Text = Translation.Get("balance.invalid");
        }
    }

    private void Redraw()
    {
        _balanceButton.Label = Translation.Get("balance.run");
    }
}
