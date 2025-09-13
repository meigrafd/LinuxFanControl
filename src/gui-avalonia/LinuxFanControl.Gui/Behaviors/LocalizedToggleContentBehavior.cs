// (c) 2025 LinuxFanControl contributors. MIT License.
using Avalonia;
using Avalonia.Controls;
using LinuxFanControl.Gui.Services;
using System;

namespace LinuxFanControl.Gui.Behaviors
{
    /// <summary>
    /// Binds a ContentControl's Content to a localized key that depends on a boolean state
    /// without hardcoding strings in code. Usage in XAML:
    ///
    ///   <Button
    ///     behaviors:LocalizedToggleContentBehavior.State="{Binding IsDarkTheme}"
    ///     behaviors:LocalizedToggleContentBehavior.KeyWhenTrue="ui.theme.light"
    ///     behaviors:LocalizedToggleContentBehavior.KeyWhenFalse="ui.theme.dark" />
    ///
    /// Here, when State==true (dark active), the button shows the action "Light" (switch to light).
    /// Keys live only in XAML, not code.
    /// </summary>
    public static class LocalizedToggleContentBehavior
    {
        public static readonly AttachedProperty<bool> StateProperty =
        AvaloniaProperty.RegisterAttached<ContentControl, bool>(
            "State", typeof(LocalizedToggleContentBehavior));

        public static readonly AttachedProperty<string?> KeyWhenTrueProperty =
        AvaloniaProperty.RegisterAttached<ContentControl, string?>(
            "KeyWhenTrue", typeof(LocalizedToggleContentBehavior));

        public static readonly AttachedProperty<string?> KeyWhenFalseProperty =
        AvaloniaProperty.RegisterAttached<ContentControl, string?>(
            "KeyWhenFalse", typeof(LocalizedToggleContentBehavior));

        static LocalizedToggleContentBehavior()
        {
            StateProperty.Changed.AddClassHandler<ContentControl>((ctrl, e) => Update(ctrl));
            KeyWhenTrueProperty.Changed.AddClassHandler<ContentControl>((ctrl, e) => Update(ctrl));
            KeyWhenFalseProperty.Changed.AddClassHandler<ContentControl>((ctrl, e) => Update(ctrl));

            // update on language change
            LocalizationService.Instance.LanguageChanged += (_, __) =>
            {
                // Brute-force: update all open windows' content controls that use attached props
                foreach (var w in TopLevel.TopLevelInstances)
                {
                    if (w is not null)
                        Walk(w);
                }
            };
        }

        private static void Walk(Visual? v)
        {
            if (v is null) return;
            if (v is ContentControl cc &&
                (cc.IsSet(StateProperty) || cc.IsSet(KeyWhenTrueProperty) || cc.IsSet(KeyWhenFalseProperty)))
            {
                Update(cc);
            }

            if (v is Panel pnl)
            {
                foreach (var c in pnl.Children) Walk(c);
            }
            else if (v is ContentControl c2 && c2.Content is Visual cv)
            {
                Walk(cv);
            }
        }

        public static bool GetState(ContentControl d) => d.GetValue(StateProperty);
        public static void SetState(ContentControl d, bool value) => d.SetValue(StateProperty, value);

        public static string? GetKeyWhenTrue(ContentControl d) => d.GetValue(KeyWhenTrueProperty);
        public static void SetKeyWhenTrue(ContentControl d, string? value) => d.SetValue(KeyWhenTrueProperty, value);

        public static string? GetKeyWhenFalse(ContentControl d) => d.GetValue(KeyWhenFalseProperty);
        public static void SetKeyWhenFalse(ContentControl d, string? value) => d.SetValue(KeyWhenFalseProperty, value);

        private static void Update(ContentControl ctrl)
        {
            try
            {
                var state = GetState(ctrl);
                var key = state ? GetKeyWhenTrue(ctrl) : GetKeyWhenFalse(ctrl);
                ctrl.Content = key is null ? null : LocalizationProvider.Instance[key];
            }
            catch { /* ignore */ }
        }
    }
}
