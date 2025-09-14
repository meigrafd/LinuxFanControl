using System;
using System.Collections.ObjectModel;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;

namespace LinuxFanControl.Gui.Behaviors
{
    public static class DragReorderBehavior
    {
        public static readonly AttachedProperty<bool> IsEnabledProperty =
        AvaloniaProperty.RegisterAttached<DragReorderBehavior, ItemsControl, bool>(
            "IsEnabled",
            defaultValue: false,
                notifying: OnIsEnabledChanged);

        public static bool GetIsEnabled(ItemsControl control) =>
        control.GetValue(IsEnabledProperty);

        public static void SetIsEnabled(ItemsControl control, bool value) =>
        control.SetValue(IsEnabledProperty, value);

        private static void OnIsEnabledChanged(AvaloniaObject d, bool oldValue, bool newValue)
        {
            if (d is not ItemsControl itemsControl)
                return;

            if (newValue)
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
            if (sender is not ItemsControl itemsControl)
                return;

            // Only start drag if behavior is enabled
            if (!itemsControl.GetValue(IsEnabledProperty))
                return;

            // TODO: Identify the item under pointer and initiate drag
        }

        private static void OnPointerReleased(object? sender, PointerReleasedEventArgs e)
        {
            if (sender is not ItemsControl itemsControl)
                return;

            if (!itemsControl.GetValue(IsEnabledProperty))
                return;

            // Ensure we have the underlying collection
            if (itemsControl.Items is ItemCollection itemCollection &&
                itemCollection.SourceCollection is ObservableCollection<object> list)
            {
                // TODO: Determine sourceIndex and targetIndex, then reorder:
                // var item = list[sourceIndex];
                // list.RemoveAt(sourceIndex);
                // list.Insert(targetIndex, item);
            }
        }
    }
}
