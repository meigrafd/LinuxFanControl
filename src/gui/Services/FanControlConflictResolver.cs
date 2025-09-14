using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlConflictResolver : VBox
{
    private readonly RpcClient _rpc;

    private Button _scanButton;
    private TreeView _conflictView;
    private ListStore _conflictStore;
    private Label _statusLabel;

    public FanControlConflictResolver()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        _scanButton = new Button(Translation.Get("conflict.scan"));
        _scanButton.Clicked += (_, _) => ScanConflicts();
        PackStart(_scanButton, false, false, 0);

        _conflictStore = new ListStore(typeof(string), typeof(string));
        _conflictView = new TreeView(_conflictStore);

        var colRule = new TreeViewColumn { Title = Translation.Get("conflict.rule") };
        colRule.PackStart(new CellRendererText(), true);
        colRule.AddAttribute(colRule.CellRenderers[0], "text", 0);
        _conflictView.AppendColumn(colRule);

        var colIssue = new TreeViewColumn { Title = Translation.Get("conflict.issue") };
        colIssue.PackStart(new CellRendererText(), true);
        colIssue.AddAttribute(colIssue.CellRenderers[0], "text", 1);
        _conflictView.AppendColumn(colIssue);

        var scroll = new ScrolledWindow();
        scroll.Add(_conflictView);
        scroll.SetSizeRequest(500, 250);
        PackStart(scroll, true, true, 0);

        _statusLabel = new Label();
        PackStart(_statusLabel, false, false, 0);
    }

    private void ScanConflicts()
    {
        _conflictStore.Clear();

        var response = _rpc.SendRequest("scanRuleConflicts");
        if (response is JsonObject obj && obj["result"] is JsonArray conflicts)
        {
            if (conflicts.Count == 0)
            {
                _statusLabel.Text = Translation.Get("conflict.clean");
            }
            else
            {
                foreach (var c in conflicts)
                {
                    var rule = c?["rule"]?.ToString() ?? "?";
                    var issue = c?["issue"]?.ToString() ?? "?";
                    _conflictStore.AppendValues(rule, issue);
                }
                _statusLabel.Text = Translation.Get("conflict.found") + $": {conflicts.Count}";
            }
        }
        else
        {
            _statusLabel.Text = Translation.Get("conflict.error");
        }
    }

    private void Redraw()
    {
        _scanButton.Label = Translation.Get("conflict.scan");
    }
}
