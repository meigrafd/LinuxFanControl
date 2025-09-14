using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using LinuxFanControl.Gui.ViewModels;
using LinuxFanControl.Gui.Views.Dialogs;

namespace LinuxFanControl.Gui.Views
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            DataContext = new MainWindowViewModel();
            this.FindControl<Button>("BtnSetup")!.Click += async (_, __) =>
            {
                var dlg = new SetupDialog();
                await dlg.ShowDialog(this);
            };
        }
        private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
    }
}
