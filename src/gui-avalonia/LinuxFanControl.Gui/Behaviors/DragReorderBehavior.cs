using System;
using System.Collections.ObjectModel;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;

namespace LinuxFanControl.Gui.Behaviors
{
    public static class DragReorderBehavior
    {
        public static readonly AttachedProperty<bool> IsEnabledProperty =
            AvaloniaProperty.RegisterAttached<ItemsControl, bool>("IsEnabled", typeof(DragReorderBehavior));

        public static void SetIsEnabled(ItemsControl control, bool value) => control.SetValue(IsEnabledProperty, value);
        public static bool GetIsEnabled(ItemsControl control) => control.GetValue(IsEnabledProperty);

        static DragReorderBehavior()
        {
            IsEnabledProperty.Changed.Subscribe(args =>
            {
                if (args.Sender is ItemsControl ic)
                {
                    if ((bool)args.NewValue!)
                    {
                        ic.PointerPressed += OnPressed;
                        ic.AddHandler(DragDrop.DropEvent, OnDrop);
                        ic.AddHandler(DragDrop.DragOverEvent, OnDragOver);
                    }
                    else
                    {
                        ic.PointerPressed -= OnPressed;
                        ic.RemoveHandler(DragDrop.DropEvent, OnDrop);
                        ic.RemoveHandler(DragDrop.DragOverEvent, OnDragOver);
                    }
                }
            });
        }

        private static void OnPressed(object? sender, PointerPressedEventArgs e)
        {
            if (sender is not ItemsControl ic) return;
            if (e.GetCurrentPoint(ic).Properties.IsLeftButtonPressed)
            {
                var item = (e.Source as Control)?.DataContext;
                if (item == null) return;
                var data = new DataObject();
                data.Set("lfc-item", item);
                DragDrop.DoDragDrop(e, data, DragDropEffects.Move);
            }
        }

        private static void OnDragOver(object? sender, DragEventArgs e)
        {
            if (!e.Data.Contains("lfc-item")) e.DragEffects = DragDropEffects.None;
            else e.DragEffects = DragDropEffects.Move;
        }

        private static void OnDrop(object? sender, DragEventArgs e)
        {
            if (sender is not ItemsControl ic) return;
            if (!e.Data.Contains("lfc-item")) return;
            var item = e.Data.Get("lfc-item");
            if (item == null) return;
            if (ic.Items is not ObservableCollection<object> items) return;

            // compute target index from pointer position
            int target = items.Count - 1;
            var pos = e.GetPosition(ic);
            for (int i = 0; i < items.Count; i++)
            {
                var c = ic.ContainerFromIndex(i) as Control;
                if (c is null) continue;
                var b = c.Bounds;
                var p = c.TranslatePoint(new Avalonia.Point(0,0), ic);
                if (p.HasValue)
                {
                    var rect = new Rect(p.Value, b.Size);
                    if (pos.X < rect.Center.X && pos.Y < rect.Bottom) { target = i; break; }
                }
            }

            var srcIdx = items.IndexOf(item);
            if (srcIdx < 0 || target < 0) return;
            if (srcIdx == target) return;
            items.RemoveAt(srcIdx);
            target = Math.Min(target, items.Count);
            items.Insert(target, item);
        }
    }
}
