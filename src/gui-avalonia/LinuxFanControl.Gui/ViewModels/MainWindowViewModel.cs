using System;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.ViewModels
{
    public class MainWindowViewModel
    {
        public string Title => LocalizationService.T("app.title", "Linux Fan Control");
    }
}
