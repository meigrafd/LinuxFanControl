using Gtk;
using FanControl.Gui.Services;
using System.Text.Json.Nodes;

namespace FanControl.Gui.Tools;

public class FanControlProfileValidator : VBox
{
    private readonly RpcClient _rpc;

    private ComboBoxText _profileSelector;
    private Button _validateButton;
    private TextView _resultView;

    public FanControlProfileValidator()
    {
        Spacing = 10;
        _rpc = new RpcClient();

        Translation.LanguageChanged += Redraw;

        BuildUi();
        LoadProfiles();
    }

    private void BuildUi()
    {
        _profileSelector = new ComboBoxText();
        PackStart(_profileSelector, false, false, 0);

        _validateButton = new Button(Translation.Get("validate.run"));
        _validateButton.Clicked += (_, _) => ValidateProfile();
        PackStart(_validateButton, false, false, 0);

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

    private void LoadProfiles()
    {
        _profileSelector.RemoveAll();

        var response = _rpc.SendRequest("listProfiles");
        if (response is JsonObject obj && obj["result"] is JsonArray profiles)
        {
            foreach (var p in profiles)
            {
                var name = p?.ToString() ?? "?";
                _profileSelector.AppendText(name);
            }

            if (_profileSelector.Children.Length > 0)
                _profileSelector.Active = 0;
        }
    }

    private void ValidateProfile()
    {
        var profile = _profileSelector.ActiveText;
        if (!string.IsNullOrEmpty(profile))
        {
            var payload = new JsonObject { ["name"] = profile };
            var response = _rpc.SendRequest("validateProfile", payload);
            if (response is JsonObject obj && obj["result"] is JsonArray issues)
            {
                if (issues.Count == 0)
                {
                    _resultView.Buffer.Text = Translation.Get("validate.clean");
                }
                else
                {
                    var lines = string.Join("\n", issues.Select(i => "• " + i?.ToString()));
                    _resultView.Buffer.Text = Translation.Get("validate.issues") + "\n" + lines;
                }
            }
            else
            {
                _resultView.Buffer.Text = Translation.Get("validate.error");
            }
        }
        else
        {
            _resultView.Buffer.Text = Translation.Get("validate.invalid");
        }
    }

    private void Redraw()
    {
        _validateButton.Label = Translation.Get("validate.run");
    }
}
