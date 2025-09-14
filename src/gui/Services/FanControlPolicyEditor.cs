using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlPolicyEditor : VBox
{
    private readonly RpcClient _rpc;

    private Entry _policyNameEntry;
    private TextView _policyTextView;
    private Button _applyButton;
    private Label _statusLabel;

    public FanControlPolicyEditor()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        foreach (var child in Children)
            Remove(child);

        _policyNameEntry = new Entry { PlaceholderText = Translation.Get("policy.name") };
        PackStart(_policyNameEntry, false, false, 0);

        _policyTextView = new TextView
        {
            WrapMode = WrapMode.Word,
            Buffer = new TextBuffer(new TextTagTable())
        };
        var scroll = new ScrolledWindow();
        scroll.Add(_policyTextView);
        scroll.SetSizeRequest(400, 150);
        PackStart(scroll, true, true, 0);

        _applyButton = new Button(Translation.Get("policy.apply"));
        _applyButton.Clicked += (_, _) => ApplyPolicy();
        PackStart(_applyButton, false, false, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void ApplyPolicy()
    {
        var name = _policyNameEntry.Text;
        var text = _policyTextView.Buffer.Text;

        if (!string.IsNullOrEmpty(name) && !string.IsNullOrEmpty(text))
        {
            var payload = new JsonObject
            {
                ["name"] = name,
                ["definition"] = text
            };
            var response = _rpc.SendRequest("setControlPolicy", payload);
            if (response is JsonObject obj && obj["result"]?.ToString() == "ok")
            {
                _statusLabel.Text = Translation.Get("policy.success");
            }
            else
            {
                _statusLabel.Text = Translation.Get("policy.error");
            }
        }
        else
        {
            _statusLabel.Text = Translation.Get("policy.invalid");
        }
    }

    private void Redraw()
    {
        _applyButton.Label = Translation.Get("policy.apply");
    }
}
