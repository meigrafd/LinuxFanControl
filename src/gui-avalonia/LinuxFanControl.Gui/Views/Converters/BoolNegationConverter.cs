// (c) 2025 LinuxFanControl contributors. MIT License.

using System;
using Avalonia.Data.Converters;

namespace LinuxFanControl.Gui.Views
{
    public sealed class BoolNegationConverter : IValueConverter
    {
        public static readonly BoolNegationConverter Instance = new();

        public object? Convert(object? value, Type targetType, object? parameter, System.Globalization.CultureInfo culture)
        {
            if (value is bool b) return !b;
            return Avalonia.AvaloniaProperty.UnsetValue;
        }

        public object? ConvertBack(object? value, Type targetType, object? parameter, System.Globalization.CultureInfo culture)
        {
            if (value is bool b) return !b;
            return Avalonia.AvaloniaProperty.UnsetValue;
        }
    }
}
