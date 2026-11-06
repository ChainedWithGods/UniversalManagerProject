using System;
using CUE4Parse.FileProvider;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Objects.UObject;

namespace PakViewer.Snooper;

public static class SnooperContext
{
    public static IFileProvider? Provider { get; set; }

    public static bool ModelIsWaitingAnimation { get; set; }
    public static bool ModelIsOverwritingMaterial { get; set; }

    public static Action<string>? Status { get; set; }

    public static string ProjectName => Provider?.ProjectName ?? string.Empty;

    public static void UpdateStatus(string message) => Status?.Invoke(message);

    public static bool TryLoadObject<T>(string objectPath, out T export) where T : UObject
    {
        export = default!;
        if (Provider == null || string.IsNullOrWhiteSpace(objectPath))
            return false;

        try
        {
            export = Provider.LoadPackageObject<T>(objectPath);
            return export != null;
        }
        catch
        {
            export = default!;
            return false;
        }
    }
}
