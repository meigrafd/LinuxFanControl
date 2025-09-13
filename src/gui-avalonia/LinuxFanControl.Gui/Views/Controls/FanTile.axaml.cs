// (c) 2025 LinuxFanControl contributors. MIT License.

using Avalonia.Controls;
using Avalonia.Input;

namespace LinuxFanControl.Gui.Views.Controls
{
    public partial class FanTile : UserControl
    {
        public FanTile()
        {
            InitializeComponent();
            AddHandler(PointerPressedEvent, OnPointerPressed, handledEventsToo: true);
        }

        private async void OnPointerPressed(object? sender, PointerPressedEventArgs e)
        {
            if (DataContext is not LinuxFanControl.Gui.ViewModels.FanTileViewModel vm)
                return;

            var data = new DataObject();
            data.Set("application/x-lfc-fantile", vm.Id);
            await DragDrop.DoDragDrop(e, data, DragDropEffects.Move);
        }
    }
}
