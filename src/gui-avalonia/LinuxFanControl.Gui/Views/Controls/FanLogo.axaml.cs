// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Threading;

namespace LinuxFanControl.Gui.Views.Controls
{
    /// <summary>
    /// Small rotating fan logo. Uses a DispatcherTimer instead of XAML animations
    /// to avoid setter/target-type issues with Avalonia 11.
    /// </summary>
    public partial class FanLogo : UserControl
    {
        private readonly DispatcherTimer _timer;
        private double _angleDeg;

        public FanLogo()
        {
            InitializeComponent();

            _timer = new DispatcherTimer
            {
                Interval = TimeSpan.FromMilliseconds(16) // ~60 FPS
            };
            _timer.Tick += (_, __) => TickRotate();
            _timer.Start();
        }

        private void TickRotate()
        {
            // 360° per 1.2s -> 300°/s -> 5° per 16ms tick approx.
            _angleDeg += 360.0 * (_timer.Interval.TotalSeconds / 1.2);
            if (_angleDeg >= 360.0) _angleDeg -= 360.0;

            var rotor = this.FindControl<Canvas>("Rotor");
            if (rotor is null) return;

            if (rotor.RenderTransform is RotateTransform rt)
            {
                rt.Angle = _angleDeg;
            }
            else
            {
                rotor.RenderTransform = new RotateTransform(_angleDeg);
            }
        }

        protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
        {
            // Stop timer when control is detached to avoid leaks.
            _timer.Stop();
            base.OnDetachedFromVisualTree(e);
        }
    }
}
