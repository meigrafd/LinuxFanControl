using Gtk;
using System.Text.Json.Nodes;

public class LayoutManager
{
    public Box Root { get; } = new Box(Orientation.Vertical, 12);

    private readonly StatusBar _status = new StatusBar();
    private readonly SensorGrid _sensors = new SensorGrid();
    private readonly FanGrid _fans = new FanGrid();
    private readonly RuleEditor _rules = new RuleEditor();
    private readonly ProfileBar _profile = new ProfileBar();
    private readonly EngineControl _engine = new EngineControl();

    public LayoutManager()
    {
        Root.Append(_status);
        Root.Append(_sensors);
        Root.Append(_fans);
        Root.Append(_rules);
        Root.Append(_profile);
        Root.Append(_engine);
    }

    public void ApplyChannelData(JsonNode? data)
    {
        if (data == null)
        {
            _status.SetText(LocaleManager._("daemon.status.unreachable"));
            return;
        }

        _status.SetText(LocaleManager._("daemon.status.running"));
        _rules.LoadChannels(data);
        _fans.LoadChannels(data);
        _engine.Enable();
    }
}
