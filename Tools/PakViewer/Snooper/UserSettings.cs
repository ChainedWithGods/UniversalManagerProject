using System;
using System.IO;
using CUE4Parse.UE4.Assets.Exports.Nanite;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse_Conversion;
using CUE4Parse_Conversion.Options;
using CUE4Parse_Conversion.Textures;
using CUE4Parse.UE4.Assets.Exports;

namespace PakViewer.Snooper;

public enum ETextureExportFormat
{
    Png,
    Tga,
    Dds
}

public class TexturePlatformSettings
{
    public ETexturePlatform TexturePlatform { get; set; } = ETexturePlatform.DesktopMobile;
}

public class UserSettings
{
    private static UserSettings? _default;

    public static UserSettings Default => _default ??= new UserSettings();

    public int PreviewMaxTextureSize { get; set; } = 1024;
    public TexturePlatformSettings CurrentDir { get; } = new();
    public ExportOptions ExportOptions { get; } = new(exportMaterials: false, materialDepth: EMaterialDepth.TopLayerOnly);
    public string ModelDirectory { get; set; } = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Downloads");
    public EMaterialDepth MaterialExportFormat { get; set; } = EMaterialDepth.TopLayerOnly;
    public bool ShowSkybox { get; set; } = true;
    public bool ShowGrid { get; set; } = true;
    public bool AnimateWithRotationOnly { get; set; } = false;
    public Camera.WorldMode CameraMode { get; set; } = Camera.WorldMode.Arcball;
    public ENaniteMeshFormat NaniteMeshExportFormat { get; set; } = ENaniteMeshFormat.NoNanite;
    public bool DisableBackfaceCulling { get; set; } = false;

    public int MaxSkeletalMeshLodLevel { get; set; } = -1;

    public bool EnableDetailedTextureLogging { get; set; } = false;
}
