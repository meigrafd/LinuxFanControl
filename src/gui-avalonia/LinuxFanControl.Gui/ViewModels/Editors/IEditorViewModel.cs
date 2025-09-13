// (c) 2025 LinuxFanControl contributors. MIT License.

using System;

namespace LinuxFanControl.Gui.ViewModels
{
    public interface IEditorViewModel
    {
        string Title { get; }
        bool IsDirty { get; }
        event EventHandler? CloseRequested;
    }
}
