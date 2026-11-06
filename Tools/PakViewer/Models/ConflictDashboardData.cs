using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json.Serialization;
using PakViewer.ViewModels;

namespace PakViewer.Models;

public class ConflictDashboardData
{
    [JsonPropertyName("modName")]
    public string ModName { get; set; } = string.Empty;

    [JsonPropertyName("pakPath")]
    public string PakPath { get; set; } = string.Empty;

    [JsonPropertyName("totalFiles")]
    public int TotalFiles { get; set; }

    [JsonPropertyName("conflictCount")]
    public int ConflictCount { get; set; }

    [JsonPropertyName("highSeverityCount")]
    public int HighSeverityCount { get; set; }

    [JsonPropertyName("mediumSeverityCount")]
    public int MediumSeverityCount { get; set; }

    [JsonPropertyName("lowSeverityCount")]
    public int LowSeverityCount { get; set; }

    [JsonPropertyName("conflicts")]
    public List<ConflictFileItem> Conflicts { get; set; } = new();

    [JsonPropertyName("newFiles")]
    public List<NewFileItem> NewFiles { get; set; } = new();
}

public class ConflictFileItem : ViewModelBase
{
    private string _overrideCategoryName = string.Empty;

    [JsonPropertyName("filePath")]
    public string FilePath { get; set; } = string.Empty;

    [JsonPropertyName("fileType")]
    public string FileType { get; set; } = string.Empty;

    [JsonPropertyName("severity")]
    public string Severity { get; set; } = "Low";

    [JsonPropertyName("conflictingMods")]
    public List<string> ConflictingMods { get; set; } = new();

    public string FileName => string.IsNullOrEmpty(FilePath) ? "Unknown Asset" : Path.GetFileName(FilePath);

    public string DirectoryPath
    {
        get
        {
            if (string.IsNullOrEmpty(FilePath)) return string.Empty;
            var dir = Path.GetDirectoryName(FilePath);
            return string.IsNullOrEmpty(dir) ? string.Empty : dir.Replace('\\', '/');
        }
    }

    public string ConflictingModsText => ConflictingMods.Count > 0 ? string.Join(", ", ConflictingMods) : "None";

    public string CategoryName
    {
        get
        {
            if (!string.IsNullOrEmpty(_overrideCategoryName))
                return _overrideCategoryName;

            return FileTypeToCategoryName(FileType);
        }
        set
        {
            _overrideCategoryName = value;
            OnPropertyChanged(nameof(CategoryName));
            OnPropertyChanged(nameof(CategoryIconSource));
            OnPropertyChanged(nameof(IconGlyph));
            OnPropertyChanged(nameof(SeverityTitle));
        }
    }

    public string CategoryIconSource => CategoryNameToIconSource(CategoryName);

    public static string CategoryNameToIconSource(string cat) => cat switch
    {
        "Physics Asset"                                     => "ms-appx:///Icons/Physics.ico",
        "Skeleton"                                          => "ms-appx:///Icons/StaticSK.ico",
        "Animation Sequence" or "Animations"                => "ms-appx:///Icons/Animations.ico",
        "Skeletal Mesh"                                     => "ms-appx:///Icons/Skeleton.ico",
        "Static Mesh"                                       => "ms-appx:///Icons/Model.ico",
        "Material Instance" or "Material" or "Materials"    => "ms-appx:///Icons/Materials.ico",
        "Texture 2D" or "Textures"                          => "ms-appx:///Icons/STextures.ico",
        "Audio & Sound" or "Audio" or "Sound"               => "ms-appx:///Icons/MSounds.ico",
        "UI Widget"                                         => "ms-appx:///Icons/UI.ico",
        "Blueprint" or "Rig Blueprints"                     => "ms-appx:///Icons/Blueprint.ico",
        "Data Table" or "Data Asset" or "Item Prefab Data"  => "ms-appx:///Icons/data.ico",
        "Niagara VFX" or "Particles"                        => "ms-appx:///Icons/atom.ico",
        "Level Map" or "Worlds/Levels"                      => "ms-appx:///Icons/World.ico",
        "Config" or "Configuration"                         => "ms-appx:///Icons/Config.ico",
        _                                                   => "ms-appx:///Icons/Item.ico"
    };

    public static string FileTypeToCategoryName(string fileType) => fileType switch
    {
        "SkeletalMesh"   => "Skeletal Mesh",
        "StaticMesh"     => "Static Mesh",
        "PhysicsAsset"   => "Physics Asset",
        "Skeleton"       => "Skeleton",
        "Animation"      => "Animation Sequence",
        "Material"       => "Material Instance",
        "Texture"        => "Texture 2D",
        "UIWidget"       => "UI Widget",
        "Blueprint"      => "Blueprint",
        "DataTable"      => "Data Table",
        "DataAsset"      => "Data Asset",
        "ParticleVFX"    => "Niagara VFX",
        "Audio"          => "Audio & Sound",
        "Font"           => "Font",
        "Map"            => "Level Map",
        "Localization"   => "Localization",
        "Config"         => "Config",
        "Asset"          => "Unreal Asset",
        "Other"          => "Other File",
        ""               => "Unreal Asset",
        _                => fileType
    };

    public string IconGlyph
    {
        get
        {
            var cat = CategoryName;
            return cat switch
            {
                "Item Prefab Data" or "Character Item Data" or "Data Asset" or "Data Table" => "\uE8F1",
                "Skeletal Mesh" or "Static Mesh" => "\uE91B",
                "Physics Asset" => "\uE7B8",
                "Skeleton" or "Animation Sequence" => "\uE805",
                "Material Instance" => "\uE790",
                "Texture 2D" => "\uE8B9",
                "UI Widget" or "Blueprint" => "\uE943",
                "Audio & Sound" => "\uE8D6",
                "Niagara VFX" => "\uE945",
                _ => "\uE8A5"
            };
        }
    }

    public string SeverityTitle
    {
        get
        {
            var cat = CategoryName;
            return cat switch
            {
                "Skeletal Mesh" or "Static Mesh"  => "CRITICAL MESH",
                "Physics Asset"                    => "PHYSICS OVERRIDE",
                "Skeleton"                         => "SKELETON OVERRIDE",
                "Animation Sequence"               => "ANIMATION OVERRIDE",
                "Material Instance"                => "MATERIAL OVERRIDE",
                "Texture 2D"                       => "TEXTURE OVERRIDE",
                "UI Widget"                        => "UI OVERRIDE",
                "Niagara VFX"                      => "VFX OVERRIDE",
                "Audio & Sound"                    => "AUDIO OVERRIDE",
                "Data Asset" or "Data Table"       => "DATA OVERRIDE",
                "Blueprint"                        => "BLUEPRINT OVERRIDE",
                "Level Map"                        => "MAP OVERRIDE",
                _ => Severity.Equals("High", StringComparison.OrdinalIgnoreCase) ? "CRITICAL OVERRIDE" : "ASSET OVERRIDE"
            };
        }
    }

    public string SeverityColorHex
    {
        get
        {
            var cat = CategoryName;
            if (Severity.Equals("High", StringComparison.OrdinalIgnoreCase) ||
                cat.Contains("Mesh") || cat.Contains("Physics") ||
                cat == "Blueprint" || cat == "Skeleton" || cat == "Animation Sequence" ||
                cat == "Data Table" || cat == "Level Map")
                return "#EF4444";
            if (cat.Contains("Material") || cat.Contains("Texture"))
                return "#F59E0B";
            return "#3B82F6";
        }
    }

    public bool IsMeshOrCritical =>
        Severity.Equals("High", StringComparison.OrdinalIgnoreCase) ||
        CategoryName.Contains("Mesh") || CategoryName.Contains("Physics") ||
        CategoryName == "Blueprint" || CategoryName == "Skeleton" ||
        CategoryName == "Animation Sequence" || CategoryName == "Data Table";
    public bool IsMaterialOrTexture => CategoryName.Contains("Material") || CategoryName.Contains("Texture");
}

public class NewFileItem
{
    [JsonPropertyName("filePath")]
    public string FilePath { get; set; } = string.Empty;

    [JsonPropertyName("fileType")]
    public string FileType { get; set; } = string.Empty;

    public string FileName => string.IsNullOrEmpty(FilePath) ? "Unknown Asset" : Path.GetFileName(FilePath);
    public string DirectoryPath => string.IsNullOrEmpty(FilePath) ? string.Empty : (Path.GetDirectoryName(FilePath) ?? string.Empty).Replace('\\', '/');
}

public class RivalModSummary
{
    public string ModName { get; set; } = string.Empty;
    public int CollidingFilesCount { get; set; }
    public string PercentText { get; set; } = "0%";
    public double PercentValue { get; set; } = 0.0;
    public string CollidingFilesText => $"{CollidingFilesCount} overlapping files";
}

public class CategoryFilterChip : ViewModelBase
{
    private bool _isSelected;

    public string CategoryTag { get; set; } = "All";
    public string DisplayText { get; set; } = "All (0)";

    public bool IsSelected
    {
        get => _isSelected;
        set
        {
            if (SetProperty(ref _isSelected, value))
            {
                OnPropertyChanged(nameof(ButtonStyle));
            }
        }
    }

    public Microsoft.UI.Xaml.Style? ButtonStyle => IsSelected ? (Microsoft.UI.Xaml.Style)Microsoft.UI.Xaml.Application.Current.Resources["AccentButtonStyle"] : null;
}
