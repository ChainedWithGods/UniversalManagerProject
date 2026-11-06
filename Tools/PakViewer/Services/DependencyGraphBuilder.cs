using System.Collections.Generic;
using System.Linq;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Assets.Exports.StaticMesh;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Assets.Exports.Animation;
using PakViewer.Models;
using PakViewer.Helpers;
using PakViewer.Snooper.Models;
using PakViewer.Snooper.Shading;

namespace PakViewer.Services;

public static class DependencyGraphBuilder
{
    public static DependencyNode BuildSkeletalGraph(SkeletalModel model, USkeletalMesh original, string? vfsName)
    {
        var root = new DependencyNode
        {
            Name = original.Name,
            Path = original.GetPathName(),
            Type = DependencyNodeType.Mesh,
            Resolution = DependencyResolution.Resolved,
            VfsName = vfsName
        };

        if (original.Skeleton.TryLoad(out var skelObj) && skelObj is USkeleton skeleton)
        {
            root.Children.Add(new DependencyNode
            {
                Name = skeleton.Name,
                Path = skeleton.GetPathName(),
                Type = DependencyNodeType.Skeleton,
                Resolution = DependencyResolution.Resolved
            });
        }
        else if (!original.Skeleton.IsNull)
        {

            root.Children.Add(new DependencyNode
            {
                Name = original.Skeleton.Name,
                Path = original.Skeleton.ResolvedObject?.GetPathName() ?? original.Skeleton.Name,
                Type = DependencyNodeType.Skeleton,
                Resolution = DependencyResolution.Missing
            });
        }

        for (int m = 0; m < model.Materials.Length && m < original.SkeletalMaterials.Length; m++)
        {
            var material = model.Materials[m];
            var slotName = original.SkeletalMaterials[m].MaterialSlotName?.Text;
            if (string.IsNullOrEmpty(slotName)) slotName = $"Slot_{m}";

            var slotNode = new DependencyNode
            {
                Name = slotName,
                Path = material.Path,
                Type = DependencyNodeType.MaterialSlot,
                Resolution = material.Name == "" || material.Parameters.IsNull
                    ? DependencyResolution.Missing
                    : DependencyResolution.Resolved
            };

            if (material.Path?.StartsWith("Fallback:") == true)
            {
                slotNode.Resolution = DependencyResolution.Fallback;
            }

            AddTextureChildren(slotNode, material);

            root.Children.Add(slotNode);
        }

        return root;
    }

    public static DependencyNode BuildStaticGraph(StaticModel model, UStaticMesh original, string? vfsName)
    {
        var root = new DependencyNode
        {
            Name = original.Name,
            Path = original.GetPathName(),
            Type = DependencyNodeType.Mesh,
            Resolution = DependencyResolution.Resolved,
            VfsName = vfsName
        };

        var slotNames = original.StaticMaterials?
            .Select(sm => sm.MaterialSlotName?.Text)
            .ToArray() ?? System.Array.Empty<string>();

        for (int m = 0; m < model.Materials.Length && m < slotNames.Length; m++)
        {
            var material = model.Materials[m];
            var slotName = !string.IsNullOrEmpty(slotNames[m]) ? slotNames[m] : $"Slot_{m}";

            var slotNode = new DependencyNode
            {
                Name = slotName,
                Path = material.Path,
                Type = DependencyNodeType.MaterialSlot,
                Resolution = material.Name == "" || material.Parameters.IsNull
                    ? DependencyResolution.Missing
                    : DependencyResolution.Resolved
            };

            if (material.Path?.StartsWith("Fallback:") == true)
            {
                slotNode.Resolution = DependencyResolution.Fallback;
            }

            AddTextureChildren(slotNode, material);

            root.Children.Add(slotNode);
        }

        return root;
    }

    private static void AddTextureChildren(DependencyNode slotNode, Material material)
    {
        if (material.Parameters.Textures == null || material.Parameters.Textures.Count == 0)
            return;

        foreach (var texParam in material.Parameters.Textures)
        {
            var texture = texParam.Value;
            if (texture == null) continue;

            var isFallback = material.Path?.StartsWith("Fallback:") == true;
            var paramName = texParam.Key;

            slotNode.Children.Add(new DependencyNode
            {
                Name = $"{paramName}: {texture.GetObjectName()}",
                Path = texture.GetObjectPathName(),
                Type = DependencyNodeType.Texture,
                Resolution = isFallback
                    ? DependencyResolution.Fallback
                    : DependencyResolution.Resolved
            });
        }
    }

    public static DependencyNode BuildVfsSummary(IEnumerable<DependencyNode> meshNodes)
    {
        var summary = new DependencyNode
        {
            Name = "Asset Summary",
            Type = DependencyNodeType.Mesh,
            Resolution = DependencyResolution.Resolved
        };

        var resolved = 0;
        var fallback = 0;
        var missing = 0;

        foreach (var mesh in meshNodes)
        {
            foreach (var slot in mesh.Children)
            {
                if (slot.Type != DependencyNodeType.MaterialSlot) continue;

                switch (slot.Resolution)
                {
                    case DependencyResolution.Resolved: resolved++; break;
                    case DependencyResolution.Fallback: fallback++; break;
                    case DependencyResolution.Missing: missing++; break;
                }
            }
        }

        summary.Children.Add(new DependencyNode
        {
            Name = $"Resolved: {resolved}",
            Type = DependencyNodeType.MaterialSlot,
            Resolution = DependencyResolution.Resolved
        });
        summary.Children.Add(new DependencyNode
        {
            Name = $"Fallback: {fallback}",
            Type = DependencyNodeType.MaterialSlot,
            Resolution = DependencyResolution.Fallback
        });
        summary.Children.Add(new DependencyNode
        {
            Name = $"Missing: {missing}",
            Type = DependencyNodeType.MaterialSlot,
            Resolution = DependencyResolution.Missing
        });

        return summary;
    }
}
