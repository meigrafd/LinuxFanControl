using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.Behaviors
{
    // NOTE: Non-static owner type is required for Avalonia RegisterAttached<> generic.
    public class LocalizedToggleContentBehavior
    {
        public static readonly AttachedProperty<string?> KeyOnProperty =
        AvaloniaProperty.RegisterAttached<LocalizedToggleContentBehavior, ToggleButton, string?>("KeyOn");

        public static readonly AttachedProperty<string?> KeyOffProperty =
        AvaloniaProperty.RegisterAttached<LocalizedToggleContentBehavior, ToggleButton, string?>("KeyOff");

        static LocalizedToggleContentBehavior()
        {
            KeyOnProperty.Changed.Subscribe(args =>
            {
                if (args.Sender is ToggleButton tb) UpdateContent(tb);
            });
                KeyOffProperty.Changed.Subscribe(args =>
                {
                    if (args.Sender is ToggleButton tb) UpdateContent(tb);
                });

                    ToggleButton.IsCheckedProperty.Changed.Subscribe(args =>
                    {
                        if (args.Sender is ToggleButton tb) UpdateContent(tb);
                    });
        }

        public static void SetKeyOn(AvaloniaObject element, string? value) => element.SetValue(KeyOnProperty, value);
        public static string? GetKeyOn(AvaloniaObject element) => element.GetValue(KeyOnProperty);

        public static void SetKeyOff(AvaloniaObject element, string? value) => element.SetValue(KeyOffProperty, value);
        public static string? GetKeyOff(AvaloniaObject element) => element.GetValue(KeyOffProperty);

        private static void UpdateContent(ToggleButton tb)
        {
            if (tb is not ContentControl cc) return;
            var key = (tb.IsChecked == true) ? GetKeyOn(tb) : GetKeyOff(tb);
            if (string.IsNullOrWhiteSpace(key)) return;

            var text = LocalizationService.GetString(key!);
            cc.Content = string.IsNullOrEmpty(text) ? key : text;
        }
    }
}
