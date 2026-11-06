using Microsoft.UI.Xaml.Data;
using System;

namespace PakViewer.Converters;

public class EnumToIntConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, string language)
    {
        if (value is Enum enumValue)
        {
            return System.Convert.ToInt32(enumValue);
        }
        return 0;
    }

    public object ConvertBack(object value, Type targetType, object parameter, string language)
    {
        if (value is int intValue && targetType.IsEnum)
        {
            return Enum.ToObject(targetType, intValue);
        }
        return value;
    }
}
