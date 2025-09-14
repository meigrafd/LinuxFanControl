// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Threading;

namespace LinuxFanControl.Gui.Views.Controls
{
    /// <summary>Rotating fan logo with improved contrast.</summary>
    public partial class FanLogo : UserControl
    {
        private readonly DispatcherTimer _timer;
        private double _angleDeg;

        public FanLogo()
        {
            InitializeComponent();
            _timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(16) };
            _timer.Tick += (_, __) => TickRotate();
            _timer.Start();
        }

        private void TickRotate()
        {
            _angleDeg += 360.0 * _timer.Interval.TotalSeconds;
            if (_angleDeg >= 360.0) _angleDeg -= 360.0;

            var rotor = this.FindControl<Canvas>("Rotor");
            if (rotor is null) return;

            if (rotor.RenderTransform is RotateTransform rt) rt.Angle = _angleDeg;
            else rotor.RenderTransform = new RotateTransform(_angleDeg);
        }

        protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
        {
            _timer.Stop();
            base.OnDetachedFromVisualTree(e);
        }
    }
}
