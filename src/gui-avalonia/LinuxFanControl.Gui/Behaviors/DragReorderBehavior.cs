// (c) 2025 LinuxFanControl contributors. MIT License.
#nullable enable
using System;
using System.Collections.Generic;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;

namespace LinuxFanControl.Gui.Behaviors
{
    public class DragReorderBehavior
    {
        private Panel? _panel;
        private Control? _draggedItem;
        private Point _dragStart;

        public void Attach(Panel panel)
        {
            _panel = panel;
            foreach (var child in panel.Children.OfType<Control>())
            {
                child.PointerPressed += OnPointerPressed;
                child.PointerMoved += OnPointerMoved;
                child.PointerReleased += OnPointerReleased;
            }
        }

        private void OnPointerPressed(object? sender, PointerPressedEventArgs e)
        {
            if (sender is Control control)
            {
                _draggedItem = control;
                _dragStart = e.GetPosition(_panel);
            }
        }

        private void OnPointerMoved(object? sender, PointerEventArgs e)
        {
            if (_draggedItem == null || _panel == null)
                return;

            var currentPosition = e.GetPosition(_panel);
            var deltaY = currentPosition.Y - _dragStart.Y;

            if (Math.Abs(deltaY) > 10)
            {
                var index = _panel.Children.IndexOf(_draggedItem);
                var targetIndex = deltaY < 0 ? index - 1 : index + 1;

                if (targetIndex >= 0 && targetIndex < _panel.Children.Count)
                {
                    _panel.Children.Remove(_draggedItem);
                    _panel.Children.Insert(targetIndex, _draggedItem);
                    _dragStart = currentPosition;
                }
            }
        }

        private void OnPointerReleased(object? sender, PointerReleasedEventArgs e)
        {
            _draggedItem = null;
        }

        public void Detach()
        {
            if (_panel == null)
                return;

            foreach (var child in _panel.Children.OfType<Control>())
            {
                child.PointerPressed -= OnPointerPressed;
                child.PointerMoved -= OnPointerMoved;
                child.PointerReleased -= OnPointerReleased;
            }

            _panel = null;
        }

        public static DragReorderBehavior? GetBehavior(Panel panel)
        {
            return panel.GetValue(BehaviorProperty);
        }

        public static void SetBehavior(Panel panel, DragReorderBehavior? value)
        {
            panel.SetValue(BehaviorProperty, value);
            value?.Attach(panel);
        }

        public static readonly AttachedProperty<DragReorderBehavior?> BehaviorProperty =
        AvaloniaProperty.RegisterAttached<Panel, DragReorderBehavior?>("Behavior");
    }
}
