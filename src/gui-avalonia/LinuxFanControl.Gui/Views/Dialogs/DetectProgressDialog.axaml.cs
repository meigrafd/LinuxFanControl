// (c) 2025 LinuxFanControl contributors. MIT License.

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Threading;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    public partial class DetectProgressDialog : Window
    {
        private string? _file;
        private CancellationTokenSource? _cts;

        public DetectProgressDialog()
        {
            InitializeComponent();
        }

        public void AttachTail(string file)
        {
            _file = file;
            _cts = new CancellationTokenSource();
            _ = TailLoopAsync(_cts.Token);
        }

        public void AppendLine(string text)
        {
            if (LogBox is null) return;
            Dispatcher.UIThread.Post(() =>
            {
                LogBox.Text += text + Environment.NewLine;
                LogBox.CaretIndex = LogBox.Text?.Length ?? 0;
            });
        }

        private async Task TailLoopAsync(CancellationToken ct)
        {
            if (string.IsNullOrWhiteSpace(_file)) return;
            long lastLen = 0;

            while (!ct.IsCancellationRequested)
            {
                try
                {
                    if (File.Exists(_file))
                    {
                        using var fs = new FileStream(_file, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
                        if (fs.Length >= lastLen)
                        {
                            fs.Seek(lastLen, SeekOrigin.Begin);
                            using var sr = new StreamReader(fs);
                            var chunk = await sr.ReadToEndAsync();
                            if (!string.IsNullOrEmpty(chunk))
                                AppendLine(chunk.TrimEnd('\r', '\n'));
                            lastLen = fs.Length;
                        }
                        else
                        {
                            lastLen = 0; // rotated/truncated
                        }
                    }
                }
                catch { /* ignore */ }

                await Task.Delay(300, ct);
            }
        }

        private void OnClose(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
        {
            _cts?.Cancel();
            Close();
        }
    }
}
