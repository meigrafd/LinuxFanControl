using Avalonia.Controls;
using LinuxFanControl.Gui.ViewModels.Dialogs;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    // Code-behind wires the DataContext and exposes a convenient ShowDialog helper via IDialogHost.
    public partial class SetupDialog : Window, IDialogHost
    {
        public SetupDialog()
        {
            InitializeComponent();
            // Design-time DataContext is in XAML; runtime is provided by caller via ShowFor.
        }

        // Helper used by MainWindow to show and get a result.
        public static async Task<(bool Ok, bool RunDetection, string ImportPath)> ShowFor(Window owner)
        {
            var dlg = new SetupDialog
            {
                DataContext = new SetupDialogViewModel(owner)
            };

            // Store result via CloseDialog callback
            var tcs = new TaskCompletionSource<(bool, bool, string)>();
            _pendingResult = tcs;
            await dlg.ShowDialog(owner);
            return await tcs.Task;
        }

        // IDialogHost impl – invoked by VM commands
        void IDialogHost.CloseDialog(bool ok, bool runDetection, string importPath)
        {
            _pendingResult?.TrySetResult((ok, runDetection, importPath));
            Close();
        }

        static TaskCompletionSource<(bool Ok, bool RunDetection, string ImportPath)>? _pendingResult;
    }
}
