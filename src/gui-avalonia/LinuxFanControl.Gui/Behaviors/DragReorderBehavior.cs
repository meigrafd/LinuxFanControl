// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using System.Collections;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;

namespace LinuxFanControl.Gui.Behaviors
{
    public static class DragReorderBehavior
    {
        public static readonly AttachedProperty<bool> IsEnabledProperty =
        AvaloniaProperty
        .RegisterAttached<DragReorderBehavior, ItemsControl, bool>(
            "IsEnabled",
            defaultValue: false);

        public static bool GetIsEnabled(ItemsControl control) =>
        control.GetValue(IsEnabledProperty);

        public static void SetIsEnabled(ItemsControl control, bool value) =>
        control.SetValue(IsEnabledProperty, value);

        static DragReorderBehavior()
        {
            // Listen for changes to our attached property
            IsEnabledProperty.Changed.Subscribe(OnIsEnabledChanged);
        }

        private static void OnIsEnabledChanged(AvaloniaPropertyChangedEventArgs e)
        {
            if (e.Sender is not ItemsControl itemsControl)
                return;

            var enabled = (bool)e.NewValue;
            if (enabled)
            {
                itemsControl.AddHandler(InputElement.PointerPressedEvent, OnPointerPressed, RoutingStrategies.Tunnel);
                itemsControl.AddHandler(InputElement.PointerReleasedEvent, OnPointerReleased, RoutingStrategies.Tunnel);
            }
            else
            {
                itemsControl.RemoveHandler(InputElement.PointerPressedEvent, OnPointerPressed);
                itemsControl.RemoveHandler(InputElement.PointerReleasedEvent, OnPointerReleased);
            }
        }

        private static void OnPointerPressed(object? sender, PointerPressedEventArgs e)
        {
            if (sender is not ItemsControl ic || !GetIsEnabled(ic))
                return;

            // TODO: Identify item under pointer and initiate drag
        }

        private static void OnPointerReleased(object? sender, PointerReleasedEventArgs e)
        {
            if (sender is not ItemsControl ic || !GetIsEnabled(ic))
                return;

            // Use IList for reorderable source
            if (ic.Items is IList list)
            {
                // TODO: Determine sourceIndex and targetIndex, then reorder:
                // var item = list[sourceIndex];
                // list.RemoveAt(sourceIndex);
                // list.Insert(targetIndex, item);
            }
        }
    }
}
