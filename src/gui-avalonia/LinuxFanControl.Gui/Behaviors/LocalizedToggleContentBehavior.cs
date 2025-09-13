using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives; // <-- needed for ToggleButton

namespace LinuxFanControl.Gui.Behaviors;

/// <summary>
/// Minimal helper to localize a ToggleButton's Content without relying on Avalonia.Xaml.Interactivity.
/// Wire it up from code-behind (e.g., in SetupDialog) by calling:
/// LocalizedToggleContentBehavior.Attach(themeToggle, onKey: "ui.theme.dark", offKey: "ui.theme.light");
/// </summary>
public static class LocalizedToggleContentBehavior
{
    /// <summary>Attach simple on/off localization to a ToggleButton.</summary>
    public static void Attach(ToggleButton toggle, string? onKey, string? offKey)
    {
        if (toggle is null) return;

        void Update()
        {
            var key = toggle.IsChecked == true ? onKey : offKey;
            var text = LocalizationService.Get(key ?? string.Empty);
            toggle.Content = text;
        }

        // Initial apply
        Update();

        // React to user changes
        toggle.Checked += (_, __) => Update();
        toggle.Unchecked += (_, __) => Update();
        toggle.Indeterminate += (_, __) => Update();

        // React if someone flips IsChecked from code
        toggle.PropertyChanged += (_, e) =>
        {
            if (e.Property == ToggleButton.IsCheckedProperty)
                Update();
        };
    }
}
