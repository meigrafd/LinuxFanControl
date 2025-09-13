using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.Behaviors
{
    /// <summary>
    /// Simple attached behavior that toggles localized Content for ToggleButton/ToggleSwitch
    /// without relying on Avalonia.Xaml.Interactivity.
    /// Usage in XAML:
    ///   <ToggleSwitch
    ///       behaviors:LocalizedToggleContentBehavior.KeyOn="ui.toggle.on"
    ///       behaviors:LocalizedToggleContentBehavior.KeyOff="ui.toggle.off" />
    /// It listens to IsChecked changes and sets Content accordingly using LocalizationService.
    /// </summary>
    public static class LocalizedToggleContentBehavior
    {
        public static readonly AttachedProperty<string?> KeyOnProperty =
        AvaloniaProperty.RegisterAttached<LocalizedToggleContentBehavior, ToggleButton, string?>("KeyOn");

        public static readonly AttachedProperty<string?> KeyOffProperty =
        AvaloniaProperty.RegisterAttached<LocalizedToggleContentBehavior, ToggleButton, string?>("KeyOff");

        static LocalizedToggleContentBehavior()
        {
            KeyOnProperty.Changed.Subscribe(args =>
            {
                if (args.Sender is ToggleButton tb)
                    AttachOrUpdate(tb);
            });
            KeyOffProperty.Changed.Subscribe(args =>
            {
                if (args.Sender is ToggleButton tb)
                    AttachOrUpdate(tb);
            });

            // Also react to IsChecked changes globally for ToggleButton
            ToggleButton.IsCheckedProperty.Changed.Subscribe(args =>
            {
                if (args.Sender is ToggleButton tb)
                    UpdateContent(tb);
            });
        }

        public static void SetKeyOn(AvaloniaObject element, string? value)
        => element.SetValue(KeyOnProperty, value);

        public static string? GetKeyOn(AvaloniaObject element)
        => element.GetValue(KeyOnProperty);

        public static void SetKeyOff(AvaloniaObject element, string? value)
        => element.SetValue(KeyOffProperty, value);

        public static string? GetKeyOff(AvaloniaObject element)
        => element.GetValue(KeyOffProperty);

        private static void AttachOrUpdate(ToggleButton tb)
        {
            // First-time call or property changed; just force a refresh.
            UpdateContent(tb);
        }

        private static void UpdateContent(ToggleButton tb)
        {
            // Works for ToggleSwitch because it inherits ContentControl via HeaderedContentControl in Avalonia.
            if (tb is not ContentControl cc)
                return;

            var isOn = tb.IsChecked == true;
            var key = isOn ? GetKeyOn(tb) : GetKeyOff(tb);

            if (!string.IsNullOrWhiteSpace(key))
            {
                var text = LocalizationService.GetString(key!);
                cc.Content = text ?? key;
            }
        }
    }
}
