using Gtk;
using FanControl.Gui.Services;
using System.IO;

namespace FanControl.Gui.Views;

public class DaemonLogView : ScrolledWindow
{
    private TextView _textView;
    private const string LogPath = "/var/log/lfcd.log";

    public DaemonLogView()
    {
        SetPolicy(PolicyType.Automatic, PolicyType.Automatic);
        BorderWidth = 6;

        _textView = new TextView
        {
            Editable = false,
            WrapMode = WrapMode.Word
        };

        Add(_textView);

        Translation.LanguageChanged += Redraw;
        LoadLog();
    }

    private void LoadLog()
    {
        try
        {
            if (File.Exists(LogPath))
            {
                var lines = File.ReadAllLines(LogPath);
                _textView.Buffer.Text = string.Join("\n", lines.TakeLast(100));
            }
            else
            {
                _textView.Buffer.Text = Translation.Get("log.notfound");
            }
        }
        catch
        {
            _textView.Buffer.Text = Translation.Get("log.error");
        }
    }

    private void Redraw()
    {
        LoadLog();
    }
}
