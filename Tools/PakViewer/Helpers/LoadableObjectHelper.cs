using CUE4Parse.UE4.Assets;
using CUE4Parse.UE4.Objects.UObject;

namespace PakViewer.Helpers;

public static class LoadableObjectHelper
{

    public static string GetObjectName(this ILoadableObject obj)
    {
        if (obj is null) return string.Empty;

        return obj switch
        {
            FPackageIndex index => index.Name,
            ResolvedObject resolved => resolved.Name.Text,
            _ => obj.Load()?.Name ?? string.Empty
        };
    }

    public static string GetObjectPathName(this ILoadableObject obj)
    {
        if (obj is null) return string.Empty;

        try
        {
            var loaded = obj.Load();
            if (loaded != null)
                return loaded.GetPathName();
        }
        catch (Exception)
        {

        }

        return obj switch
        {
            FPackageIndex index => index.ResolvedObject?.GetPathName() ?? index.Name,
            ResolvedObject resolved => resolved.GetPathName(),
            _ => string.Empty
        };
    }
}
