// (c) 2025 LinuxFanControl contributors. MIT License.
using System;
using System.Globalization;
using Avalonia.Data.Converters;

namespace LinuxFanControl.Gui.Converters
{
    public sealed class InverseBoolConverter : IValueConverter
    {
        public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
        => value is bool b ? !b : value;

        public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => value is bool b ? !b : value;
    }
}
