using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives; // ToggleButton
using LinuxFanControl.Gui.Services; // LocalizationService

namespace LinuxFanControl.Gui.Behaviors;

/// <summary>
/// Minimal helper to localize a ToggleButton's Content without Avalonia.Xaml.Interactivity.
/// Call once from code-behind, e.g.:
/// LocalizedToggleContentBehavior.Attach(ThemeToggle, "ui.theme.dark", "ui.theme.light");
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
            var text = LocalizationService.GetString(key ?? string.Empty);
            toggle.Content = text;
        }

        // Initial apply
        Update();

        // React on changes without deprecated events
        toggle.GetObservable(ToggleButton.IsCheckedProperty)
        .Subscribe(_ => Update());
    }
}
