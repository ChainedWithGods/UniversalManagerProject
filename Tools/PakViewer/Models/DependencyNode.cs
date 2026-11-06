using System;
using System.Collections.Generic;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Media;

namespace PakViewer.Models;

public enum DependencyNodeType
{
    Mesh,
    MaterialSlot,
    Texture,
    Skeleton
}

public enum DependencyResolution
{
    Resolved,
    Fallback,
    Missing
}

public class DependencyNode
{
    private static readonly SolidColorBrush MeshBadgeColor = new(Microsoft.UI.ColorHelper.FromArgb(255, 33, 150, 243));
    private static readonly SolidColorBrush SlotBadgeColor = new(Microsoft.UI.ColorHelper.FromArgb(255, 156, 39, 176));
    private static readonly SolidColorBrush TexBadgeColor = new(Microsoft.UI.ColorHelper.FromArgb(255, 255, 152, 0));
    private static readonly SolidColorBrush SkelBadgeColor = new(Microsoft.UI.ColorHelper.FromArgb(255, 76, 175, 80));
    private static readonly SolidColorBrush DefaultBadgeColor = new(Microsoft.UI.ColorHelper.FromArgb(255, 158, 158, 158));

    public string Name { get; set; } = "";
    public string Path { get; set; } = "";
    public DependencyNodeType Type { get; set; }
    public DependencyResolution Resolution { get; set; }
    public string? VfsName { get; set; }
    public List<DependencyNode> Children { get; set; } = new();

    public SolidColorBrush StatusColor => Resolution switch
    {
        DependencyResolution.Resolved => new SolidColorBrush(Microsoft.UI.Colors.Green),
        DependencyResolution.Fallback => new SolidColorBrush(Microsoft.UI.Colors.Orange),
        DependencyResolution.Missing => new SolidColorBrush(Microsoft.UI.Colors.Red),
        _ => new SolidColorBrush(Microsoft.UI.Colors.Gray)
    };

    public string TypeLabel => Type switch
    {
        DependencyNodeType.Mesh => "MESH",
        DependencyNodeType.MaterialSlot => "SLOT",
        DependencyNodeType.Texture => "TEX",
        DependencyNodeType.Skeleton => "SKEL",
        _ => ""
    };

    public string ResolutionLabel => Resolution switch
    {
        DependencyResolution.Resolved => "Resolved",
        DependencyResolution.Fallback => "Fallback",
        DependencyResolution.Missing => "Missing",
        _ => "Unknown"
    };

    public SolidColorBrush TypeBadgeColor => Type switch
    {
        DependencyNodeType.Mesh => MeshBadgeColor,
        DependencyNodeType.MaterialSlot => SlotBadgeColor,
        DependencyNodeType.Texture => TexBadgeColor,
        DependencyNodeType.Skeleton => SkelBadgeColor,
        _ => DefaultBadgeColor
    };

    public Visibility HasVfsName => !string.IsNullOrEmpty(VfsName) ? Visibility.Visible : Visibility.Collapsed;
}
