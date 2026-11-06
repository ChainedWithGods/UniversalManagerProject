using CUE4Parse.UE4.Assets.Exports.Material;
using PakViewer.Snooper.Models;

namespace PakViewer.Snooper.Shading;

public enum MaterialAnalysis
{

    Unknown,

    WellDefined,

    Fallback,

    NoTextures
}

public static class MaterialAnalysisExtensions
{

    public static MaterialAnalysis Analyze(UMaterialInterface unrealMaterial, CMaterialParams2 parameters)
    {
        if (parameters.IsNull)
            return MaterialAnalysis.NoTextures;

        if (parameters.Textures.Count == 0)
            return MaterialAnalysis.NoTextures;

        bool hasDiffuse = parameters.HasTopDiffuse || parameters.TryGetTexture2d(out _, CMaterialParams2.FallbackDiffuse);
        bool hasNormal = parameters.HasTopNormals || parameters.TryGetTexture2d(out _, CMaterialParams2.FallbackNormals);

        if (hasDiffuse || hasNormal)
            return MaterialAnalysis.WellDefined;

        return MaterialAnalysis.Fallback;
    }
}
