using Microsoft.UI.Xaml.Data;
using System;

namespace PakViewer.Converters;

public class FileExtensionToIconConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, string language)
    {
        if (value is string extension)
        {
            return extension.ToLower() switch
            {
                "ini" => "\uE70F",
                "json" => "\uE943",
                "txt" => "\uE8A5",
                "xml" => "\uE943",
                "cfg" => "\uE70F",
                "log" => "\uE8A5",
                "uasset" => "\uEA86",
                "umap" => "\uE909",
                "uexp" => "\uE7B8",
                "ubulk" => "\uE7B8",
                "png" => "\uE91B",
                "jpg" => "\uE91B",
                "jpeg" => "\uE91B",
                "dds" => "\uE91B",
                "tga" => "\uE91B",
                _ => "\uE8B7"
            };
        }

        return "\uE8B7";
    }

    public object ConvertBack(object value, Type targetType, object parameter, string language)
    {
        throw new NotImplementedException();
    }
}
