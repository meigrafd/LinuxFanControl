using Avalonia.Controls;
using Avalonia.Interactivity;
using LinuxFanControl.Gui.ViewModels.Dialogs;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    public partial class SetupDialog : Window
    {
        public bool ShouldRunDetection { get; private set; }
        public bool ShouldRunCalibration { get; private set; }

        public SetupDialog()
        {
            InitializeComponent();
            var vm = new SetupDialogViewModel();
            DataContext = vm;

            this.FindControl<Button>("BtnApply").Click += (_, __) =>
            {
                if (DataContext is SetupDialogViewModel m)
                {
                    ShouldRunDetection = m.RunDetection;
                    ShouldRunCalibration = m.RunCalibration;
                }
                Close(true);
            };
            this.FindControl<Button>("BtnCancel").Click += (_, __) => Close(false);
        }
    }
}
