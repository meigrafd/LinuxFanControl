// (c) 2025 LinuxFanControl contributors. MIT License.

using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using LinuxFanControl.Gui.ViewModels;

namespace LinuxFanControl.Gui.Views
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();

            if (DataContext is not MainWindowViewModel vm)
            {
                vm = new MainWindowViewModel();
                DataContext = vm;
            }

            vm.EnsureLanguagesInitialized();
            vm.EnsureThemesInitialized();
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }
    }
}
