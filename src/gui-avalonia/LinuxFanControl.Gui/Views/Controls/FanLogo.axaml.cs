using System;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using Avalonia.Media;
using Avalonia.Threading;

namespace LinuxFanControl.Gui.Views.Controls
{
    public partial class FanLogo : UserControl
    {
        private readonly DispatcherTimer _timer;
        private double _angle = 0;
        private RotateTransform _rt = new RotateTransform();

        public FanLogo()
        {
            InitializeComponent();
            var blade = this.FindControl<Shape>("Blade");
            blade.RenderTransform = _rt;
            blade.RenderTransformOrigin = new RelativePoint(0.5, 0.5, RelativeUnit.Relative);
            _timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(30) };
            _timer.Tick += (_, __) =>
            {
                _angle = (_angle + 6) % 360;
                _rt.Angle = _angle;
            };
            _timer.Start();
            this.DetachedFromVisualTree += (_, __) => _timer.Stop();
        }
        private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
    }
}
