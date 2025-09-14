using Gtk;

namespace FanControl.Gui.Widgets;

public class StatusBanner : Box
{
    private readonly Label _messageLabel;

    public StatusBanner() : base(Orientation.Horizontal, 10)
    {
        Margin = 10;
        HeightRequest = 40;

        _messageLabel = new Label("Ready");
        PackStart(_messageLabel, true, true, 0);
    }

    public void SetMessage(string message)
    {
        _messageLabel.Text = message;
    }
}
