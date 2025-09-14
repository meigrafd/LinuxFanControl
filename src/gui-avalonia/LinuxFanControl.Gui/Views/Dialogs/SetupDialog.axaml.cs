using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using LinuxFanControl.Gui.Services;
using LinuxFanControl.Gui.ViewModels.Dialogs;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    public partial class SetupDialog : Window
    {
        public SetupDialog() { InitializeComponent();
            var btnCancel = this.FindControl<Button>("BtnCancel")!;
            var btnApply = this.FindControl<Button>("BtnApply")!;
            btnCancel.Click += (_, __) => Close();
            btnApply.Click += (_, __) =>
            {
                if (DataContext is SetupDialogViewModel vm)
                {
                    LocalizationService.SetLocale(vm.SelectedLanguage);
                    ThemeManager.ApplyTheme(vm.SelectedTheme);
                    ConfigService.Save(new GuiConfig(vm.SelectedLanguage, vm.SelectedTheme));
                }
                Close();
            };
        }
        private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
    }
}
