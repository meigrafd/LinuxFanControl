// (c) 2025 LinuxFanControl contributors. MIT License.

using System;
using System.Collections.ObjectModel;
using System.IO;
using System.Reactive;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Controls;
using ReactiveUI;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels.Dialogs
{
    public sealed class SetupDialogViewModel : ReactiveObject
    {
        public Localizer L => LocalizationService.Instance.Localizer;

        public ObservableCollection<string> Themes { get; } = new() { "midnight", "light" };
        public ObservableCollection<string> Languages { get; } = new() { "en", "de" };

        private string _selectedTheme;
        public string SelectedTheme { get => _selectedTheme; set => this.RaiseAndSetIfChanged(ref _selectedTheme, value); }

        private string _selectedLanguage;
        public string SelectedLanguage { get => _selectedLanguage; set => this.RaiseAndSetIfChanged(ref _selectedLanguage, value); }

        private bool _runDetection;
        public bool RunDetection { get => _runDetection; set => this.RaiseAndSetIfChanged(ref _runDetection, value); }

        private string _importFileName = "";
        public string ImportFileName { get => _importFileName; set => this.RaiseAndSetIfChanged(ref _importFileName, value); }

        public ReactiveCommand<Unit, Unit> ImportCommand { get; }
        public ReactiveCommand<Unit, Unit> ApplyCommand  { get; }
        public ReactiveCommand<Unit, Unit> CancelCommand { get; }

        public SetupDialogViewModel()
        {
            var (theme, lang) = LocalizationService.LoadGuiConfigOrDefault();
            _selectedTheme = theme;
            _selectedLanguage = lang;

            ImportCommand = ReactiveCommand.CreateFromTask(DoImportAsync);
            ApplyCommand  = ReactiveCommand.CreateFromTask(DoApplyAsync);
            CancelCommand = ReactiveCommand.CreateFromTask(DoCancelAsync);
        }

        private async Task DoImportAsync()
        {
            if (App.Current?.ApplicationLifetime is Avalonia.Controls.ApplicationLifetimes.IClassicDesktopStyleApplicationLifetime desk &&
                desk.MainWindow != null)
            {
                var dlg = new OpenFileDialog
                {
                    Title = "Select FanControl configuration (JSON)",
                    AllowMultiple = false,
                    Filters =
                    {
                        new FileDialogFilter { Name = "JSON", Extensions = { "json" } },
                        new FileDialogFilter { Name = "All",  Extensions = { "*" } }
                    }
                };
                var res = await dlg.ShowAsync(desk.MainWindow);
                if (res != null && res.Length > 0)
                {
                    ImportFileName = res[0];
                    // TODO: call importer service here.
                }
            }
        }

        private async Task DoApplyAsync()
        {
            LocalizationService.SaveGuiConfig(SelectedTheme, SelectedLanguage);
            ThemeManager.Instance.ApplyTheme(SelectedTheme);
            LocalizationService.Instance.SetLanguage(SelectedLanguage);

            if (RunDetection)
            {
                await RunDetectionWithProgressAsync();
            }
            CloseDialog(true);
        }

        private Task DoCancelAsync()
        {
            CloseDialog(false);
            return Task.CompletedTask;
        }

        private void CloseDialog(bool result)
        {
            if (App.Current?.ApplicationLifetime is Avalonia.Controls.ApplicationLifetimes.IClassicDesktopStyleApplicationLifetime desk)
            {
                foreach (var w in desk.Windows)
                {
                    if (w is LinuxFanControl.Gui.Views.Dialogs.SetupDialog sd)
                    {
                        sd.Close(result);
                        break;
                    }
                }
            }
        }

        private async Task RunDetectionWithProgressAsync()
        {
            using var cts = new CancellationTokenSource();
            var dlg = new Views.Dialogs.DetectProgressDialog();
            dlg.AttachTail("/tmp/daemon_lfc.log");
            dlg.Show();

            try
            {
                var rpc = RpcClient.Instance.DetectCalibrateAsync(cts.Token);
                await rpc;
                dlg.AppendLine("[OK] detectCalibrate finished.");
            }
            catch (Exception ex)
            {
                dlg.AppendLine("[ERR] " + ex.Message);
            }
            finally
            {
                await Task.Delay(300);
                dlg.Close();
            }
        }
    }
}
