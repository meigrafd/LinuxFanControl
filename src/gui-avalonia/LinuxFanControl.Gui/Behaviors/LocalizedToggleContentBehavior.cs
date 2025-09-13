// (c) 2025 LinuxFanControl contributors. MIT License.
// Purpose: Swap ToggleSwitch.Content between On/Off texts when IsChecked changes.

using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Xaml.Interactivity;

namespace LinuxFanControl.Gui.Behaviors
{
    public sealed class LocalizedToggleContentBehavior : Behavior<ToggleSwitch>
    {
        public static readonly StyledProperty<string?> OnTextProperty =
        AvaloniaProperty.Register<LocalizedToggleContentBehavior, string?>(nameof(OnText));

        public static readonly StyledProperty<string?> OffTextProperty =
        AvaloniaProperty.Register<LocalizedToggleContentBehavior, string?>(nameof(OffText));

        public string? OnText
        {
            get => GetValue(OnTextProperty);
            set => SetValue(OnTextProperty, value);
        }

        public string? OffText
        {
            get => GetValue(OffTextProperty);
            set => SetValue(OffTextProperty, value);
        }

        private IDisposable? _sub;

        protected override void OnAttached()
        {
            base.OnAttached();
            if (AssociatedObject is null) return;

            // initial apply
            ApplyContent(AssociatedObject.IsChecked == true);

            // subscribe to IsChecked changes
            _sub = AssociatedObject.GetObservable(ToggleSwitch.IsCheckedProperty)
            .Subscribe(v => ApplyContent(v == true));
        }

        protected override void OnDetaching()
        {
            _sub?.Dispose();
            _sub = null;
            base.OnDetaching();
        }

        private void ApplyContent(bool isOn)
        {
            if (AssociatedObject is null) return;
            var on = OnText ?? "On";
            var off = OffText ?? "Off";
            AssociatedObject.Content = isOn ? on : off;
        }
    }
}
