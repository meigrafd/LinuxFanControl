// (c) 2025 LinuxFanControl contributors. MIT License.
using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.VisualTree;
using Avalonia.Xaml.Interactivity;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.Behaviors
{
    public sealed class LocalizedToggleContentBehavior : Behavior<ToggleSwitch>
    {
        public static readonly StyledProperty<string?> OnKeyProperty =
        AvaloniaProperty.Register<LocalizedToggleContentBehavior, string?>(nameof(OnKey));

        public static readonly StyledProperty<string?> OffKeyProperty =
        AvaloniaProperty.Register<LocalizedToggleContentBehavior, string?>(nameof(OffKey));

        public string? OnKey { get => GetValue(OnKeyProperty); set => SetValue(OnKeyProperty, value); }
        public string? OffKey { get => GetValue(OffKeyProperty); set => SetValue(OffKeyProperty, value); }

        protected override void OnAttached()
        {
            base.OnAttached();
            if (AssociatedObject is null) return;
            AssociatedObject.AttachedToVisualTree += OnLoaded;
            AssociatedObject.IsCheckedChanged += OnCheckedChanged;
        }

        protected override void OnDetaching()
        {
            if (AssociatedObject is not null)
            {
                AssociatedObject.AttachedToVisualTree -= OnLoaded;
                AssociatedObject.IsCheckedChanged -= OnCheckedChanged;
            }
            base.OnDetaching();
        }

        private void OnLoaded(object? s, VisualTreeAttachmentEventArgs e) => UpdateLabel();
        private void OnCheckedChanged(object? s, RoutedEventArgs e) => UpdateLabel();

        private void UpdateLabel()
        {
            if (AssociatedObject is null) return;
            var key = AssociatedObject.IsChecked == true ? OnKey : OffKey;
            AssociatedObject.Content = key is null ? string.Empty : LocalizationProvider.Instance[key];
        }
    }
}
