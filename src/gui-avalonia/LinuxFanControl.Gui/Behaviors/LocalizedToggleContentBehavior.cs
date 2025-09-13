// (c) 2025 LinuxFanControl contributors. MIT License.
// Purpose: Swap ToggleSwitch.Content between On/Off texts when IsChecked changes.

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

        protected override void OnAttached()
        {
            base.OnAttached();
            if (AssociatedObject is null) return;

            // initial apply
            ApplyContent(AssociatedObject.IsChecked == true);

            // observe changes without System.Reactive
            AssociatedObject.PropertyChanged += OnAssociatedPropertyChanged;
        }

        protected override void OnDetaching()
        {
            if (AssociatedObject is not null)
                AssociatedObject.PropertyChanged -= OnAssociatedPropertyChanged;

            base.OnDetaching();
        }

        private void OnAssociatedPropertyChanged(object? sender, AvaloniaPropertyChangedEventArgs e)
        {
            if (e.Property == ToggleSwitch.IsCheckedProperty)
            {
                var isOn = (bool?)e.NewValue == true;
                ApplyContent(isOn);
            }
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
