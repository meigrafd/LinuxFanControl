using Avalonia.Controls;
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

            var btnApply  = this.FindControl<Button>("BtnApply");
            var btnCancel = this.FindControl<Button>("BtnCancel");

            if (btnApply is not null)
            {
                btnApply.Click += (_, __) =>
                {
                    if (DataContext is SetupDialogViewModel m)
                    {
                        ShouldRunDetection  = m.RunDetection;
                        ShouldRunCalibration = m.RunCalibration;
                    }
                    Close(true);
                };
            }

            if (btnCancel is not null)
            {
                btnCancel.Click += (_, __) => Close(false);
            }
        }
    }
}
