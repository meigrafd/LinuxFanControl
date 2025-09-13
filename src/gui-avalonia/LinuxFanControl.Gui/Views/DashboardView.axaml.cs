// (c) 2025 LinuxFanControl contributors. MIT License.

using System.Collections.ObjectModel;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using LinuxFanControl.Gui.ViewModels;

namespace LinuxFanControl.Gui.Views
{
    public partial class DashboardView : UserControl
    {
        public DashboardView()
        {
            InitializeComponent();
            this.AttachedToVisualTree += (_, __) => WireDnD();
        }

        private void WireDnD()
        {
            if (TileBoard is null) return;
            DragDrop.SetAllowDrop(TileBoard, true);
            TileBoard.AddHandler(DragDrop.DragOverEvent, OnDragOver, handledEventsToo: true);
            TileBoard.AddHandler(DragDrop.DropEvent,     OnDrop,     handledEventsToo: true);
        }

        private void OnDragOver(object? sender, DragEventArgs e)
        {
            if (!e.Data.Contains("application/x-lfc-fantile"))
            {
                e.DragEffects = DragDropEffects.None;
                e.Handled = true;
                return;
            }
            e.DragEffects = DragDropEffects.Move;
            e.Handled = true;
        }

        private void OnDrop(object? sender, DragEventArgs e)
        {
            if (!e.Data.Contains("application/x-lfc-fantile")) return;
            var id = e.Data.Get("application/x-lfc-fantile") as string;
            if (string.IsNullOrEmpty(id)) return;

            if (DataContext is not DashboardViewModel vm) return;
            var items = vm.FanTiles as ObservableCollection<FanTileViewModel>;
            if (items is null) return;

            // Find source index
            var fromIdx = -1;
            for (int i = 0; i < items.Count; i++)
                if (items[i].Id == id) { fromIdx = i; break; }
                if (fromIdx < 0) return;

                // crude target index: append to end (simple/robust). If you want precise placement,
                // replace with hit-test of ItemContainer bounds as in previous attempt.
                var toIdx = items.Count - 1;

            if (toIdx != fromIdx)
            {
                var item = items[fromIdx];
                items.RemoveAt(fromIdx);
                if (toIdx >= items.Count) items.Add(item);
                else items.Insert(toIdx, item);
            }
            e.Handled = true;
        }
    }
}
