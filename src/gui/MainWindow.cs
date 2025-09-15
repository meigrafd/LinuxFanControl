using Gtk;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

public class MainWindow : Window
{
    private readonly JsonRpcClient _rpc;
    private readonly LayoutManager _layout;

    public MainWindow(JsonRpcClient rpc) : base()
    {
        _rpc = rpc;
        Title = LocaleManager._("window.title");
        DefaultWidth = 960;
        DefaultHeight = 640;

        _layout = new LayoutManager();
        SetChild(_layout.Root);

        Present();
        InitAsync();
    }

    private async void InitAsync()
    {
        JsonNode? response = null;

        await Task.Run(() =>
        {
            try
            {
                response = _rpc.Send("listChannels");
            }
            catch
            {
                response = null;
            }
        });

        _layout.ApplyChannelData(response);
    }
}
