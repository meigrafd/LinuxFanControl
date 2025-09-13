using System;
using System.Reflection;
using Avalonia.Controls;
using Avalonia.Controls.Primitives; // ToggleButton
using LinuxFanControl.Gui.Services; // only for typeof(LocalizationService)

namespace LinuxFanControl.Gui.Behaviors;

/// <summary>
/// Localizes a ToggleButton's Content (on/off) without binding to a specific LocalizationService API.
/// It reflects common method names on LocalizationService: GetString, Get, Translate, GetText, T.
/// Example:
///     LocalizedToggleContentBehavior.Attach(ThemeToggle, "ui.theme.dark", "ui.theme.light");
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
            toggle.Content = ResolveLocalizedString(key);
        }

        // initial apply
        Update();

        // react to IsChecked changes (no deprecated events)
        toggle.GetObservable(ToggleButton.IsCheckedProperty)
        .Subscribe(_ => Update());
    }

    /// <summary>
    /// Attempts to call LocalizationService.{GetString|Get|Translate|GetText|T}(string).
    /// Falls back to the key itself if nothing is found.
    /// </summary>
    private static string ResolveLocalizedString(string? key)
    {
        if (string.IsNullOrWhiteSpace(key))
            return string.Empty;

        var t = typeof(LocalizationService);
        const BindingFlags flags = BindingFlags.Public | BindingFlags.Static | BindingFlags.Instance;
        var candidates = new[] { "GetString", "Get", "Translate", "GetText", "T" };

        foreach (var name in candidates)
        {
            var mi = t.GetMethod(name, flags, new[] { typeof(string) });
            if (mi == null) continue;

            object? target = mi.IsStatic ? null : CreateInstanceSafe(t);
            try
            {
                var res = mi.Invoke(target, new object[] { key });
                if (res is string s) return s;
            }
            catch
            {
                // ignore and try next candidate
            }
        }

        // nothing worked -> show key
        return key;
    }

    private static object? CreateInstanceSafe(Type t)
    {
        try { return Activator.CreateInstance(t); }
        catch { return null; }
    }
}
