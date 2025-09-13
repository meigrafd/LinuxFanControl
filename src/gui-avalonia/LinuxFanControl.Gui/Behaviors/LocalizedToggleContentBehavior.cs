using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.PropertyStore;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.Behaviors
{
    /// <summary>
    /// Attached "behavior" for ToggleButton that localizes its Content
    /// based on IsChecked: true -> KeyOn, false/null -> KeyOff.
    /// Usage in XAML:
    ///   <ToggleButton
    ///     l:LocalizedToggleContentBehavior.Enable="True"
    ///     l:LocalizedToggleContentBehavior.KeyOn="ui.on"
    ///     l:LocalizedToggleContentBehavior.KeyOff="ui.off"/>
    /// </summary>
    public static class LocalizedToggleContentBehavior
    {
        public static readonly AttachedProperty<bool> EnableProperty =
        AvaloniaProperty.RegisterAttached<LocalizedToggleContentBehavior, ToggleButton, bool>("Enable");

        public static readonly AttachedProperty<string?> KeyOnProperty =
        AvaloniaProperty.RegisterAttached<LocalizedToggleContentBehavior, ToggleButton, string?>("KeyOn");

        public static readonly AttachedProperty<string?> KeyOffProperty =
        AvaloniaProperty.RegisterAttached<LocalizedToggleContentBehavior, ToggleButton, string?>("KeyOff");

        static LocalizedToggleContentBehavior()
        {
            EnableProperty.Changed.AddClassHandler<ToggleButton>((btn, _) => Wire(btn));
            KeyOnProperty.Changed.AddClassHandler<ToggleButton>((btn, _) => UpdateContent(btn));
            KeyOffProperty.Changed.AddClassHandler<ToggleButton>((btn, _) => UpdateContent(btn));
            ToggleButton.IsCheckedProperty.Changed.AddClassHandler<ToggleButton>((btn, _) => UpdateContent(btn));
        }

        public static void SetEnable(AvaloniaObject obj, bool value) => obj.SetValue(EnableProperty, value);
        public static bool GetEnable(AvaloniaObject obj) => obj.GetValue(EnableProperty);

        public static void SetKeyOn(AvaloniaObject obj, string? value) => obj.SetValue(KeyOnProperty, value);
        public static string? GetKeyOn(AvaloniaObject obj) => obj.GetValue(KeyOnProperty);

        public static void SetKeyOff(AvaloniaObject obj, string? value) => obj.SetValue(KeyOffProperty, value);
        public static string? GetKeyOff(AvaloniaObject obj) => obj.GetValue(KeyOffProperty);

        private static readonly AttachedProperty<bool> _hookedProperty =
        AvaloniaProperty.RegisterAttached<LocalizedToggleContentBehavior, ToggleButton, bool>("__hooked");

        private static void Wire(ToggleButton btn)
        {
            var enabled = GetEnable(btn);
            if (!enabled)
            {
                // optional: could detach logic, but harmless to leave wired
                return;
            }
            if (!btn.GetValue(_hookedProperty))
            {
                btn.SetValue(_hookedProperty, true);
                UpdateContent(btn);
            }
        }

        private static void UpdateContent(ToggleButton btn)
        {
            if (!GetEnable(btn)) return;

            var key = btn.IsChecked == true ? GetKeyOn(btn) : GetKeyOff(btn);
            if (string.IsNullOrWhiteSpace(key))
                return;

            // Use central localization service; falls back to key if missing
            var text = LocalizationService.Get(key!);
            btn.Content = text;
        }
    }
}
