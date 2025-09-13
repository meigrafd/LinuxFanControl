// (c) 2025 LinuxFanControl contributors. MIT License.
// Purpose: Simple behavior to swap ToggleSwitch.Content between two texts.
// No localization service dependency; bind OnText/OffText from resources if needed.

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

        protected override void OnAttached()
        {
            base.OnAttached();

            if (AssociatedObject is null)
                return;

            // initial apply
            ApplyContent(AssociatedObject.IsChecked == true);

            // react to changes
            AssociatedObject.GetObservable(ToggleSwitch.IsCheckedProperty)
            .Subscribe(checkedNow => ApplyContent(checkedNow == true));
        }

        private void ApplyContent(bool isOn)
        {
            if (AssociatedObject is null) return;
            AssociatedObject.Content = isOn ? (OnText ?? "On") : (OffText ?? "Off");
        }

        protected override void OnDetaching()
        {
            // nothing to clean up (subscriptions are weak via GetObservable pipeline)
            base.OnDetaching();
        }
    }
}
