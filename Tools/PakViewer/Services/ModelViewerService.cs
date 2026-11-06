using CUE4Parse.FileProvider;
using CUE4Parse.UE4.Assets.Exports.StaticMesh;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using PakViewer.Views;
using Serilog;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;
using Microsoft.UI.Xaml;
using System.Threading;

namespace PakViewer.Services;

public class ModelViewerService
{
    public object? CreateModelViewer(IFileProvider? provider, string filePath, out bool hasModel, bool autoOpen = false)
    {
        hasModel = false;

        if (provider == null) return null;

        try
        {
            var package = provider.LoadPackage(filePath);

            foreach (var exportLazy in package.ExportsLazy)
            {
                var obj = exportLazy?.Value;

                if (obj is UStaticMesh staticMesh)
                {
                    hasModel = true;
                    Log.Information($"Found Static Mesh: {staticMesh.Name}");
                    if (autoOpen)
                    {
                        LaunchPakViewer(provider, filePath, staticMesh.Name);
                    }
                    return null;
                }

                if (obj is USkeletalMesh skeletalMesh)
                {
                    hasModel = true;
                    Log.Information($"Found Skeletal Mesh: {skeletalMesh.Name}");
                    if (autoOpen)
                    {
                        LaunchPakViewer(provider, filePath, skeletalMesh.Name);
                    }
                    return null;
                }
            }
        }
        catch (Exception ex)
        {
            Log.Error($"Failed to load 3D model: {ex.Message}");
        }

        return null;
    }

    public static void LaunchPakViewer(IFileProvider provider, string filePath, string titleName, IntPtr parentWindow = default)
    {
        try
        {
            var gameSettings = GameWindowSettings.Default;
            var nativeSettings = new NativeWindowSettings
            {
                ClientSize = new Vector2i(1280, 720),
                Title = $"PakViewer - {titleName}",
                NumberOfSamples = 4,
                Flags = ContextFlags.ForwardCompatible,
                API = ContextAPI.OpenGL,
                APIVersion = new Version(4, 6),
                StartFocused = true
            };

            using var window = new PakViewerWindow(gameSettings, nativeSettings, provider, parentWindow);
            window.LoadFromFilePath(filePath);
            window.Run();
        }
        catch (Exception ex)
        {
            Log.Error(ex, "PakViewer error");
        }
    }

    public static void LaunchPakViewerWithMaterial(IFileProvider provider, string filePath, string titleName, MaterialPreviewData? materialData, IntPtr parentWindow = default)
    {
        try
        {
            var gameSettings = GameWindowSettings.Default;
            var nativeSettings = new NativeWindowSettings
            {
                ClientSize = new Vector2i(1280, 720),
                Title = $"PakViewer - Material: {titleName}",
                NumberOfSamples = 4,
                Flags = ContextFlags.ForwardCompatible,
                API = ContextAPI.OpenGL,
                APIVersion = new Version(4, 6),
                StartFocused = true
            };

            using var window = new PakViewerWindow(gameSettings, nativeSettings, provider, parentWindow);
            window.SetMaterialData(materialData);
            window.LoadFromFilePath(filePath);
            window.Run();
        }
        catch (Exception ex)
        {
            Log.Error(ex, "PakViewer error");
        }
    }
}
