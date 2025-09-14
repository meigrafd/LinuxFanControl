using System;
using System.Text;
using Avalonia.Controls;
using Avalonia.Interactivity;

namespace LinuxFanControl.Gui.Views.Dialogs
{
    // Progress dialog for detection/calibration.
    // No bindings required; updated via code-behind to keep it simple and robust.
    public partial class DetectProgressDialog : Window
    {
        private readonly StringBuilder _log = new();
        public bool CancelRequested { get; private set; }

        public DetectProgressDialog()
        {
            InitializeComponent();
            BtnCancel.Click += OnCancelClicked;
            BtnClose.Click  += OnCloseClicked;
        }

        private void OnCancelClicked(object? sender, RoutedEventArgs e)
        {
            CancelRequested = true;
            BtnCancel.IsEnabled = false;
            BtnCancel.Content = "Canceling…";
        }

        private void OnCloseClicked(object? sender, RoutedEventArgs e)
        {
            Close();
        }

        public void SetIndeterminate(bool indeterminate)
        {
            Bar.IsIndeterminate = indeterminate;
        }

        public void SetProgress(int current, int total, string? label = null)
        {
            if (total <= 0)
            {
                Bar.IsIndeterminate = true;
                return;
            }

            Bar.IsIndeterminate = false;
            Bar.Minimum = 0;
            Bar.Maximum = total;
            Bar.Value   = Math.Clamp(current, 0, total);
            LblStatus.Text = $"{current}/{total} – {label ?? string.Empty}";
        }

        public void AppendLog(string line)
        {
            if (string.IsNullOrEmpty(line)) return;
            if (_log.Length > 0) _log.AppendLine();
            _log.Append(line);
            LogView.Text = _log.ToString();
        }

        public void ShowCloseButton()
        {
            BtnClose.IsVisible = true;
            BtnCancel.IsVisible = false;
        }
    }
}
