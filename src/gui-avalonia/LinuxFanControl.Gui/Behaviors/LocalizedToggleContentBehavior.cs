using System;
using Avalonia;
using Avalonia.Controls.Primitives;
using Avalonia.Xaml.Interactivity;
using LinuxFanControl.Gui.Services;

namespace LinuxFanControl.Gui.Behaviors
{
    /// <summary>
    /// Behavior that localizes a ToggleButton's Content based on two localization keys
    /// for the Off/On states (unchecked/checked). No hardcoded strings in XAML.
    /// Usage in XAML:
    ///   <ToggleButton>
    ///     <i:Interaction.Behaviors>
    ///       <behaviors:LocalizedToggleContentBehavior OffKey="ui.theme.light" OnKey="ui.theme.dark"/>
    ///     </i:Interaction.Behaviors>
    ///   </ToggleButton>
    /// </summary>
    public sealed class LocalizedToggleContentBehavior : Behavior<ToggleButton>
    {
        public static readonly StyledProperty<string?> OffKeyProperty =
        AvaloniaProperty.Register<LocalizedToggleContentBehavior, string?>(nameof(OffKey));

        public static readonly StyledProperty<string?> OnKeyProperty =
        AvaloniaProperty.Register<LocalizedToggleContentBehavior, string?>(nameof(OnKey));

        public string? OffKey
        {
            get => GetValue(OffKeyProperty);
            set => SetValue(OffKeyProperty, value);
        }

        public string? OnKey
        {
            get => GetValue(OnKeyProperty);
            set => SetValue(OnKeyProperty, value);
        }

        private IDisposable? _subChecked;
        private IDisposable? _subLang;

        protected override void OnAttached()
        {
            base.OnAttached();

            if (AssociatedObject is null)
                return;

            // react to checked state changes
            _subChecked = AssociatedObject
            .GetObservable(ToggleButton.IsCheckedProperty)
            .Subscribe(_ => UpdateContent());

            // react to language changes (optional event)
            _subLang = LocalizationService.LanguageChanged.Subscribe(_ => UpdateContent());

            UpdateContent();
        }

        protected override void OnDetaching()
        {
            base.OnDetaching();
            _subChecked?.Dispose();
            _subChecked = null;
            _subLang?.Dispose();
            _subLang = null;
        }

        private void UpdateContent()
        {
            var tb = AssociatedObject;
            if (tb is null) return;

            var key = (tb.IsChecked == true) ? OnKey : OffKey;
            var text = LocalizationService.T(key ?? string.Empty, key ?? string.Empty);
            tb.Content = text;
        }
    }
}
