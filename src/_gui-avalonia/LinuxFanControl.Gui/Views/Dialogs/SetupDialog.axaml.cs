using Avalonia.Controls;
using Avalonia.Interactivity;
using LinuxFanControl.Gui.ViewModels.Dialogs;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    public partial class SetupDialog : Window
    {
        public SetupDialog()
        {
            InitializeComponent();
            DataContext = new SetupDialogViewModel();

            this.FindControl<Button>("BtnCancel")!.Click += (_, __) => Close(false);
            this.FindControl<Button>("BtnApply")!.Click += async (_, __) =>
            {
                var vm = (SetupDialogViewModel)DataContext!;
                var ok = await vm.ApplyAsync();
                Close(ok ? (bool?)true : (bool?)false);
            };
        }
    }
}
