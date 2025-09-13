using System;
using Avalonia;
using Avalonia.Controls.Primitives;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.Behaviors
{
    /// <summary>
    /// Attached behavior to localize a ToggleButton's Content for Off/On states.
    /// No dependency on Avalonia.Xaml.Interactivity.
    ///
    /// Usage in XAML:
    ///   xmlns:behaviors="clr-namespace:LinuxFanControl.Gui.Behaviors"
    ///   <ToggleButton
    ///       behaviors:LocalizedToggleContentBehavior.OffKey="ui.theme.light"
    ///       behaviors:LocalizedToggleContentBehavior.OnKey="ui.theme.dark"/>
    /// </summary>
    public static class LocalizedToggleContentBehavior
    {
        public static readonly AttachedProperty<string?> OffKeyProperty =
        AvaloniaProperty.RegisterAttached<LocalizedToggleContentBehavior, ToggleButton, string?>(
            "OffKey", defaultValue: null, notifying: OnKeyChanged);

        public static readonly AttachedProperty<string?> OnKeyProperty =
        AvaloniaProperty.RegisterAttached<LocalizedToggleContentBehavior, ToggleButton, string?>(
            "OnKey", defaultValue: null, notifying: OnKeyChanged);

        // Internal storage for our subscriptions so we can dispose on change/detach.
        private static readonly AttachedProperty<IDisposable?> SubscriptionsProperty =
        AvaloniaProperty.RegisterAttached<LocalizedToggleContentBehavior, ToggleButton, IDisposable?>(
            "Subscriptions");

        public static void SetOffKey(ToggleButton element, string? value) => element.SetValue(OffKeyProperty, value);
        public static string? GetOffKey(ToggleButton element) => element.GetValue(OffKeyProperty);

        public static void SetOnKey(ToggleButton element, string? value) => element.SetValue(OnKeyProperty, value);
        public static string? GetOnKey(ToggleButton element) => element.GetValue(OnKeyProperty);

        private static void OnKeyChanged(IAvaloniaObject obj, bool _)
        {
            if (obj is ToggleButton tb)
                EnsureSubscribed(tb);
        }

        private static void EnsureSubscribed(ToggleButton tb)
        {
            // dispose previous subscriptions if any
            tb.GetValue(SubscriptionsProperty)?.Dispose();

            // subscribe to IsChecked and to language changes
            var subChecked = tb.GetObservable(ToggleButton.IsCheckedProperty)
            .Subscribe(_ => UpdateContent(tb));

            var subLang = LocalizationService.LanguageChanged
            .Subscribe(_ => UpdateContent(tb));

            tb.SetValue(SubscriptionsProperty, new PairDisposable(subChecked, subLang));

            // initial update
            UpdateContent(tb);
        }

        private static void UpdateContent(ToggleButton tb)
        {
            var offKey = tb.GetValue(OffKeyProperty);
            var onKey  = tb.GetValue(OnKeyProperty);
            var key    = (tb.IsChecked == true) ? onKey : offKey;

            // Fallback: show the key itself when missing
            var text = LocalizationService.T(key ?? string.Empty, key ?? string.Empty);
            tb.Content = text;
        }

        private sealed class PairDisposable : IDisposable
        {
            private IDisposable? _a;
            private IDisposable? _b;
            public PairDisposable(IDisposable a, IDisposable b) { _a = a; _b = b; }
            public void Dispose()
            {
                try { _a?.Dispose(); } catch { /* ignore */ }
                try { _b?.Dispose(); } catch { /* ignore */ }
                _a = null; _b = null;
            }
        }
    }
}
