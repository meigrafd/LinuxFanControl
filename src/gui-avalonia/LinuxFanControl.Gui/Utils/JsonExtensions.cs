using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text.Json;

namespace LinuxFanControl.Gui.Utils
{
    /// <summary>
    /// Convenience extensions to make System.Text.Json easier to use
    /// without changing existing call sites.
    /// </summary>
    public static class JsonExtensions
    {
        public static IEnumerable<JsonElement> AsArray(this JsonElement element)
        {
            if (element.ValueKind == JsonValueKind.Array)
            {
                foreach (var item in element.EnumerateArray())
                    yield return item;
            }
            else
            {
                yield break;
            }
        }

        public static bool TryGet(this JsonElement obj, string propertyName, out JsonElement value)
        {
            if (obj.ValueKind == JsonValueKind.Object && obj.TryGetProperty(propertyName, out value))
                return true;

            value = default;
            return false;
        }

        public static string? AsString(this JsonElement el)
        {
            return el.ValueKind == JsonValueKind.String ? el.GetString() : el.ToString();
        }

        public static double? AsDouble(this JsonElement el)
        {
            if (el.ValueKind == JsonValueKind.Number)
                return el.GetDouble();

            if (el.ValueKind == JsonValueKind.String &&
                double.TryParse(el.GetString(), NumberStyles.Float, CultureInfo.InvariantCulture, out var d))
                return d;

            return null;
        }

        public static int? AsInt(this JsonElement el)
        {
            if (el.ValueKind == JsonValueKind.Number && el.TryGetInt32(out var v))
                return v;

            if (el.ValueKind == JsonValueKind.String && int.TryParse(el.GetString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out var i))
                return i;

            return null;
        }

        public static bool? AsBool(this JsonElement el)
        {
            if (el.ValueKind == JsonValueKind.True)  return true;
            if (el.ValueKind == JsonValueKind.False) return false;
            if (el.ValueKind == JsonValueKind.String && bool.TryParse(el.GetString(), out var b))
                return b;
            return null;
        }
    }
}
