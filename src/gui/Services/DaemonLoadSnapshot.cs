using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class DaemonLoadSnapshot : VBox
{
    private readonly RpcClient _rpc;

    private Button _snapshotButton;
    private TextView _resultView;

    public DaemonLoadSnapshot()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
    }

    private void BuildUi()
    {
        _snapshotButton = new Button(Translation.Get("snapshot.run"));
        _snapshotButton.Clicked += (_, _) => CaptureSnapshot();
        PackStart(_snapshotButton, false, false, 0);

        _resultView = new TextView
        {
            Editable = false,
            WrapMode = WrapMode.Word
        };
        var scroll = new ScrolledWindow();
        scroll.Add(_resultView);
        scroll.SetSizeRequest(500, 200);
        PackStart(scroll, true, true, 0);
    }

    private void CaptureSnapshot()
    {
        var response = _rpc.SendRequest("getLoadSnapshot");
        if (response is JsonObject obj && obj["result"] is JsonObject result)
        {
            _resultView.Buffer.Text = result.ToJsonString(new System.Text.Json.JsonSerializerOptions
            {
                WriteIndented = true
            });
        }
        else
        {
            _resultView.Buffer.Text = Translation.Get("snapshot.error");
        }
    }

    private void Redraw()
    {
        _snapshotButton.Label = Translation.Get("snapshot.run");
    }
}
