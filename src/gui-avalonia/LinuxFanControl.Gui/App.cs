// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using Avalonia.Threading;

namespace LinuxFanControl.Gui
{
    public static class App
    {
        public static void UI(Action action)
        {
            if (Dispatcher.UIThread.CheckAccess())
                action();
            else
                Dispatcher.UIThread.Post(action);
        }
    }
}
