using Avalonia.Controls;
using Avalonia.Interactivity;
using LinuxFanControl.Gui.Views.Dialogs;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.Views
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            // apply default theme once at startup so the app isn't black
            if (string.IsNullOrEmpty(ThemeManager.CurrentTheme))
                ThemeManager.Apply(ThemeManager.DefaultTheme());

            // wire menu buttons if you expose them in XAML:
            var setupBtn = this.FindControl<Button>("BtnSetup");
            if (setupBtn is not null)
                setupBtn.Click += OnSetupClick;
        }

        async void OnSetupClick(object? sender, RoutedEventArgs e)
        {
            var dlg = new SetupDialog();
            var result = await dlg.ShowDialog<bool?>(this);
            if (result == true)
            {
                // TODO: if RunDetection set, kick off daemon detection here via RpcClient
            }
        }
    }
}
