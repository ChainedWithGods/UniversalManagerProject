using System;
using System.Collections.Generic;
using CUE4Parse.UE4.Assets;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse_Conversion.Options;
using Serilog;

namespace PakViewer.Snooper.Shading;

public static class MaterialParameterResolver
{
    private const int MaxParentDepth = 8;

    public static int ResolveFromParents(CMaterialParams2 parameters, UMaterialInterface material, EMaterialDepth depth)
    {
        if (parameters.Textures.Count == 0) return 0;

        var unresolved = new List<string>();
        foreach (var (key, value) in parameters.Textures)
        {
            if (!value.TryLoad<UTexture2D>(out _)) unresolved.Add(key);
        }

        if (unresolved.Count == 0) return 0;

        var recovered = 0;
        var visited = new HashSet<UMaterialInterface>(ReferenceEqualityComparer.Instance);
        var current = material;

        for (var level = 0; level < MaxParentDepth; level++)
        {
            var parent = LoadParent(current);
            if (parent == null || !visited.Add(parent)) break;
            current = parent;

            var parentParams = new CMaterialParams2();
            parent.GetParams(parentParams, depth);

            foreach (var key in unresolved)
            {
                if (!parentParams.Textures.TryGetValue(key, out var candidate)) continue;
                if (!candidate.TryLoad<UTexture2D>(out var texture)) continue;

                parameters.Textures[key] = new ResolvedLoadedObject(texture);
                recovered++;
            }

            unresolved.RemoveAll(key =>
                parameters.Textures.TryGetValue(key, out var value) && value.TryLoad<UTexture2D>(out _));
            if (unresolved.Count == 0) break;
        }

        if (recovered > 0)
            Log.Information($"Material '{material.Name}': recovered {recovered} texture parameter(s) from parent material");

        return recovered;
    }

    private static UMaterialInterface? LoadParent(UMaterialInterface material)
    {
        if (material is not UMaterialInstance instance || instance.Parent == null) return null;
        if (material.Owner is not IPackage package) return null;

        var resolved = package.ResolvePackageIndex(instance.Parent);
        if (resolved == null) return null;

        return resolved.TryLoad(out var parent) ? parent as UMaterialInterface : null;
    }
}
