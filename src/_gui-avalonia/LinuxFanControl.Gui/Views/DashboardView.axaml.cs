using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.VisualTree;

namespace LinuxFanControl.Gui.Views
{
    /// <summary>
    /// Dashboard with a tile board that supports drag & drop reordering.
    /// Notes:
    /// - Use attached property DragDrop.AllowDrop instead of ItemsControl.AllowDrop.
    /// - Avoid explicit IVisual references to keep compilation happy on all targets.
    /// </summary>
    public partial class DashboardView : UserControl
    {
        private ItemsControl? _board;

        public DashboardView()
        {
            InitializeComponent();

            _board = this.FindControl<ItemsControl>("TileBoard");
            if (_board != null)
            {
                // Enable drop via attached property
                DragDrop.SetAllowDrop(_board, true);

                // Centralized DnD handlers on the board
                _board.AddHandler(DragDrop.DragOverEvent, OnDragOver, RoutingStrategies.Tunnel | RoutingStrategies.Bubble);
                _board.AddHandler(DragDrop.DropEvent, OnDrop, RoutingStrategies.Tunnel | RoutingStrategies.Bubble);

                // Start dragging when pointer is pressed on a tile
                _board.AddHandler(InputElement.PointerPressedEvent, OnPointerPressed, RoutingStrategies.Tunnel | RoutingStrategies.Bubble);
            }
        }

        // Start a drag when user presses on a tile
        private async void OnPointerPressed(object? sender, PointerPressedEventArgs e)
        {
            if (_board == null) return;

            // Find the nearest Control with a DataContext under the pointer
            var src = (e.Source as Control)?.GetSelfAndVisualAncestors()
            .OfType<Control>()
            .FirstOrDefault(c => c.DataContext != null);
            var vm = src?.DataContext;
            if (vm == null) return;

            // Only start drag for left button
            if (!e.GetCurrentPoint(this).Properties.IsLeftButtonPressed) return;

            var data = new DataObject();
            data.Set("application/x-lfc-tile-vm", vm);

            await DragDrop.DoDragDrop(e, data, DragDropEffects.Move);
        }

        // Allow move effect while dragging over the board
        private void OnDragOver(object? sender, DragEventArgs e)
        {
            if (!e.Data.Contains("application/x-lfc-tile-vm"))
            {
                e.DragEffects = DragDropEffects.None;
                e.Handled = true;
                return;
            }
            e.DragEffects = DragDropEffects.Move;
            e.Handled = true;
        }

        // Reorder collection on drop
        private void OnDrop(object? sender, DragEventArgs e)
        {
            if (_board == null) return;

            if (!e.Data.Contains("application/x-lfc-tile-vm")) return;
            var draggedVm = e.Data.Get("application/x-lfc-tile-vm");
            if (draggedVm == null) return;

            // Resolve the list bound to ItemsSource: expects DataContext.FanTiles : IList/ObservableCollection
            var tilesProp = DataContext?.GetType().GetProperty("FanTiles");
            if (tilesProp?.GetValue(DataContext) is not System.Collections.IList list) return;

            // Determine target VM by inspecting the visual under the pointer
            var targetControl = (e.Source as Control)?.GetSelfAndVisualAncestors()
            .OfType<Control>()
            .FirstOrDefault(c => c.DataContext != null);
            var targetVm = targetControl?.DataContext;

            int from = IndexOf(list, draggedVm);
            if (from < 0) return;

            int to = targetVm != null ? IndexOf(list, targetVm) : list.Count - 1;
            if (to < 0) to = list.Count - 1;

            if (from == to) return;

            Move(list, from, to);
            e.Handled = true;
        }

        private static int IndexOf(System.Collections.IList list, object vm)
        {
            for (int i = 0; i < list.Count; i++)
            {
                if (ReferenceEquals(list[i], vm) || Equals(list[i], vm))
                    return i;
            }
            return -1;
        }

        private static void Move(System.Collections.IList list, int from, int to)
        {
            object item = list[from]!;
            list.RemoveAt(from);
            if (to >= list.Count) list.Add(item);
            else list.Insert(to, item);
        }
    }
}
