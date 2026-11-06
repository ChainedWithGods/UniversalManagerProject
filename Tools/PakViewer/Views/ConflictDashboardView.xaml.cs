using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using PakViewer;
using PakViewer.Models;
using Serilog;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;

namespace PakViewer.Views;

public class View3DRequestedEventArgs : EventArgs
{
    public FileNodeViewModel? TargetNode { get; set; }
    public string? TargetPath { get; set; }

    public View3DRequestedEventArgs(FileNodeViewModel? node = null, string? path = null)
    {
        TargetNode = node;
        TargetPath = path;
    }
}

public sealed partial class ConflictDashboardView : UserControl
{
    public event EventHandler<View3DRequestedEventArgs>? View3DRequested;
    public event EventHandler? ExplorerRequested;

    private ConflictDashboardData? _dashboardData;
    private List<ConflictFileItem> _allConflicts = new();
    private List<FileNodeViewModel> _fileTree = new();
    private ObservableCollection<CategoryFilterChip> _filterChips = new();
    private string _modFilePath = string.Empty;
    private string _currentFilterTag = "All";

    public ConflictDashboardView()
    {
        this.InitializeComponent();
    }

    public void Populate(string modFilePath, List<FileNodeViewModel> fileTree, ConflictDashboardData? dashboardData)
    {
        _modFilePath = modFilePath;
        _fileTree = fileTree;
        _dashboardData = dashboardData;

        var modName = _dashboardData?.ModName;
        if (string.IsNullOrEmpty(modName))
        {
            modName = Path.GetFileNameWithoutExtension(modFilePath);
        }

        DashboardModNameText.Text = modName;
        DashboardModSubtext.Text = modFilePath;

        _allConflicts = _dashboardData?.Conflicts ?? new List<ConflictFileItem>();

        SynchronizeCUE4ParseCategories(fileTree);

        var criticalCount = _allConflicts.Count(c => c.IsMeshOrCritical);
        StatCriticalCountText.Text = criticalCount.ToString("N0");

        var visualCount = _allConflicts.Count(c => c.IsMaterialOrTexture);
        StatVisualCountText.Text = visualCount.ToString("N0");

        PopulateRivalModsSummary(modName);

        var totalConflictCount = _allConflicts.Count > 0 ? _allConflicts.Count : (_dashboardData?.ConflictCount ?? 0);
        DashboardImpactBadgeText.Text = totalConflictCount > 0 ? $"{totalConflictCount:N0} File Collisions" : "No Collisions";

        PopulateFilterChips();

        ApplyFilters();

        PopulateInspect3DFlyout();

        PopulateCategorySummary(fileTree);
    }

    private void SynchronizeCUE4ParseCategories(List<FileNodeViewModel> fileTree)
    {
        if (_allConflicts == null || _allConflicts.Count == 0)
            return;

        var allNodes = new List<FileNodeViewModel>();
        if (fileTree != null && fileTree.Count > 0)
        {
            CollectAllFiles(fileTree, allNodes);
        }

        var nodeMap = new Dictionary<string, FileNodeViewModel>(StringComparer.OrdinalIgnoreCase);
        foreach (var node in allNodes)
        {
            var cleanPath = (node.FullPath ?? string.Empty).Replace('\\', '/').TrimStart('/');
            nodeMap[cleanPath] = node;
            nodeMap[node.Name] = node;
        }

        foreach (var conflict in _allConflicts)
        {
            if (!string.IsNullOrEmpty(conflict.FileType) &&
                !conflict.FileType.Equals("Asset", StringComparison.OrdinalIgnoreCase) &&
                !conflict.FileType.Equals("Other", StringComparison.OrdinalIgnoreCase))
            {
                conflict.CategoryName = ConflictFileItem.FileTypeToCategoryName(conflict.FileType);
                continue;
            }

            var cleanConflictPath = conflict.FilePath.Replace('\\', '/').TrimStart('/');
            if (nodeMap.TryGetValue(cleanConflictPath, out var matchedNode) ||
                nodeMap.TryGetValue(conflict.FileName, out matchedNode))
            {
                conflict.CategoryName = GetAssetCategoryFromFileNode(matchedNode);
            }
            else if (!string.IsNullOrEmpty(conflict.FileType))
            {
                conflict.CategoryName = ConflictFileItem.FileTypeToCategoryName(conflict.FileType);
            }
        }
    }

    private void PopulateFilterChips()
    {
        _filterChips.Clear();

        var allChip = new CategoryFilterChip
        {
            CategoryTag = "All",
            DisplayText = $"All ({_allConflicts.Count})",
            IsSelected = string.Equals(_currentFilterTag, "All", StringComparison.OrdinalIgnoreCase)
        };
        _filterChips.Add(allChip);

        var categoryGroups = _allConflicts
            .GroupBy(c => c.CategoryName)
            .OrderByDescending(g => g.Count())
            .ToList();

        foreach (var group in categoryGroups)
        {
            var catName = group.Key;
            var count = group.Count();
            var chip = new CategoryFilterChip
            {
                CategoryTag = catName,
                DisplayText = $"{catName} ({count})",
                IsSelected = string.Equals(_currentFilterTag, catName, StringComparison.OrdinalIgnoreCase)
            };
            _filterChips.Add(chip);
        }

        if (DynamicFilterChipsItemsControl != null)
        {
            DynamicFilterChipsItemsControl.ItemsSource = _filterChips;
        }
    }

    private void DynamicFilterChip_Click(object sender, RoutedEventArgs e)
    {
        if (sender is Button btn && btn.Tag is string tag)
        {
            _currentFilterTag = tag;

            foreach (var chip in _filterChips)
            {
                chip.IsSelected = string.Equals(chip.CategoryTag, tag, StringComparison.OrdinalIgnoreCase);
            }

            ApplyFilters();
        }
    }

    private void PopulateRivalModsSummary(string currentModName)
    {
        var dict = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);

        foreach (var conflict in _allConflicts)
        {
            foreach (var rival in conflict.ConflictingMods)
            {
                if (string.Equals(rival, currentModName, StringComparison.OrdinalIgnoreCase)) continue;

                if (!dict.ContainsKey(rival)) dict[rival] = 0;
                dict[rival]++;
            }
        }

        var totalCount = _allConflicts.Count > 0 ? _allConflicts.Count : 1;
        var summaries = dict.Select(kvp =>
        {
            var pct = Math.Min(100.0, (kvp.Value / (double)totalCount) * 100.0);
            return new RivalModSummary
            {
                ModName = kvp.Key,
                CollidingFilesCount = kvp.Value,
                PercentText = $"{pct:F0}%",
                PercentValue = pct
            };
        }).OrderByDescending(s => s.CollidingFilesCount).ToList();

        if (RivalModsItemsControl != null)
        {
            RivalModsItemsControl.ItemsSource = summaries;
        }

        if (summaries.Count > 0)
        {
            var topRival = summaries[0];
            StatPrimaryRivalText.Text = topRival.ModName;
            StatPrimaryRivalSubtext.Text = $"{topRival.CollidingFilesCount} overlapping files detected";

            if (StatPrimaryRivalBadgeText != null)
                StatPrimaryRivalBadgeText.Text = $"{topRival.PercentText} COLLISION";
        }
        else
        {
            StatPrimaryRivalText.Text = "None";
            StatPrimaryRivalSubtext.Text = "No mod collisions detected";

            if (StatPrimaryRivalBadgeText != null)
                StatPrimaryRivalBadgeText.Text = "0% COLLISION";
        }
    }

    private void ApplyFilters()
    {
        if (DashboardConflictsListView == null || DashboardConflictsEmptyState == null)
            return;

        if (_allConflicts == null || _allConflicts.Count == 0)
        {
            DashboardConflictsListView.ItemsSource = null;
            DashboardConflictsEmptyState.Visibility = Visibility.Visible;
            return;
        }

        var search = DashboardConflictSearch?.Text?.Trim().ToLowerInvariant() ?? "";

        var filtered = _allConflicts.Where(c =>
        {
            if (!string.Equals(_currentFilterTag, "All", StringComparison.OrdinalIgnoreCase))
            {
                if (!string.Equals(c.CategoryName, _currentFilterTag, StringComparison.OrdinalIgnoreCase))
                    return false;
            }

            if (!string.IsNullOrEmpty(search))
            {
                var matchFile = c.FileName.ToLowerInvariant().Contains(search);
                var matchPath = c.FilePath.ToLowerInvariant().Contains(search);
                var matchCategory = c.CategoryName.ToLowerInvariant().Contains(search);
                var matchMods = c.ConflictingModsText.ToLowerInvariant().Contains(search);
                if (!matchFile && !matchPath && !matchCategory && !matchMods) return false;
            }

            return true;
        }).ToList();

        DashboardConflictsListView.ItemsSource = filtered;
        DashboardConflictsEmptyState.Visibility = filtered.Count == 0 ? Visibility.Visible : Visibility.Collapsed;

        if (ExplorerAssetCountText != null)
        {
            ExplorerAssetCountText.Text = filtered.Count.ToString("N0");
        }
    }

    private void DashboardConflictSearch_TextChanged(object sender, TextChangedEventArgs e)
    {
        ApplyFilters();
    }

    private void DashboardView3D_Click(object sender, RoutedEventArgs e)
    {
        View3DRequested?.Invoke(this, new View3DRequestedEventArgs());
    }

    private void PopulateInspect3DFlyout()
    {
        if (Inspect3DFlyout == null) return;

        Inspect3DFlyout.Items.Clear();

        var autoItem = new MenuFlyoutItem
        {
            Text = "Auto-Detect Primary Model"
        };
        autoItem.Click += (s, e) => View3DRequested?.Invoke(this, new View3DRequestedEventArgs());
        Inspect3DFlyout.Items.Add(autoItem);

        var meshCandidates = new List<(string Name, string Path, FileNodeViewModel? Node)>();
        var seenPaths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        foreach (var conflict in _allConflicts)
        {
            var isMesh = conflict.IsMeshOrCritical ||
                         conflict.CategoryName.Contains("Mesh", StringComparison.OrdinalIgnoreCase) ||
                         conflict.CategoryName.Contains("Physics", StringComparison.OrdinalIgnoreCase) ||
                         conflict.FileType.Equals("SkeletalMesh", StringComparison.OrdinalIgnoreCase) ||
                         conflict.FileType.Equals("StaticMesh", StringComparison.OrdinalIgnoreCase) ||
                         conflict.FileType.Equals("PhysicsAsset", StringComparison.OrdinalIgnoreCase);

            if (isMesh && !seenPaths.Contains(conflict.FilePath))
            {
                seenPaths.Add(conflict.FilePath);
                meshCandidates.Add((conflict.FileName, conflict.FilePath, null));
            }
        }

        if (_fileTree != null && _fileTree.Count > 0)
        {
            var allNodes = new List<FileNodeViewModel>();
            CollectAllFiles(_fileTree, allNodes);
            foreach (var node in allNodes)
            {
                if (node.IsSkeletalMesh == true || node.IsPhysicsAsset == true ||
                    node.Name.StartsWith("SK_", StringComparison.OrdinalIgnoreCase) ||
                    node.Name.StartsWith("SKM_", StringComparison.OrdinalIgnoreCase) ||
                    node.Name.StartsWith("SM_", StringComparison.OrdinalIgnoreCase) ||
                    node.Name.StartsWith("PHYS_", StringComparison.OrdinalIgnoreCase))
                {
                    if (!seenPaths.Contains(node.FullPath))
                    {
                        seenPaths.Add(node.FullPath);
                        meshCandidates.Add((node.Name, node.FullPath, node));
                    }
                }
            }
        }

        if (meshCandidates.Count > 0)
        {
            Inspect3DFlyout.Items.Add(new MenuFlyoutSeparator());

            foreach (var candidate in meshCandidates)
            {
                var item = new MenuFlyoutItem
                {
                    Text = candidate.Name
                };
                var targetNode = candidate.Node;
                var targetPath = candidate.Path;
                item.Click += (s, e) => View3DRequested?.Invoke(this, new View3DRequestedEventArgs(targetNode, targetPath));
                Inspect3DFlyout.Items.Add(item);
            }
        }
    }

    private void NavExplorer_Click(object sender, RoutedEventArgs e)
    {
        ExplorerRequested?.Invoke(this, EventArgs.Empty);
    }

    private async void DashboardExportReport_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            var savePicker = new Windows.Storage.Pickers.FileSavePicker();
            savePicker.FileTypeChoices.Add("Text File", new List<string> { ".txt" });
            savePicker.SuggestedFileName = $"{DashboardModNameText.Text}_Conflict_Report";

            var appWindow = (Application.Current as App)?.MainWindowInstance;
            if (appWindow != null)
            {
                var hwnd = WinRT.Interop.WindowNative.GetWindowHandle(appWindow);
                WinRT.Interop.InitializeWithWindow.Initialize(savePicker, hwnd);
            }

            var file = await savePicker.PickSaveFileAsync();
            if (file != null)
            {
                var sb = new System.Text.StringBuilder();
                sb.AppendLine("===============================================");
                sb.AppendLine($"  {DashboardModNameText.Text.ToUpper()} CONFLICT DIAGNOSTICS");
                sb.AppendLine("===============================================");
                sb.AppendLine($"Mod Path: {_modFilePath}");
                sb.AppendLine($"Total Conflicts: {_allConflicts.Count}");
                sb.AppendLine($"3D Meshes & Physics: {StatCriticalCountText.Text}");
                sb.AppendLine($"Materials & Textures: {StatVisualCountText.Text}");
                sb.AppendLine($"Primary Rival Mod: {StatPrimaryRivalText.Text}");
                sb.AppendLine();
                sb.AppendLine("CONFLICTING ASSETS:");
                foreach (var c in _allConflicts)
                {
                    sb.AppendLine($" - [{c.CategoryName}] {c.FileName} ({c.DirectoryPath}) -> Collides with: {c.ConflictingModsText}");
                }

                await Windows.Storage.FileIO.WriteTextAsync(file, sb.ToString());
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to export report from dashboard");
        }
    }

    private void PopulateCategorySummary(List<FileNodeViewModel> fileTree)
    {
        var allFiles = new List<FileNodeViewModel>();
        CollectAllFiles(fileTree, allFiles);

        var dict = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        foreach (var file in allFiles)
        {
            var categoryName = GetAssetCategoryFromFileNode(file);
            if (!dict.ContainsKey(categoryName)) dict[categoryName] = 0;
            dict[categoryName]++;
        }

        var summaryItems = dict.Select(kvp => new {
            Name = kvp.Key,
            Count = kvp.Value,
            IconSource = ConflictFileItem.CategoryNameToIconSource(kvp.Key)
        }).OrderByDescending(x => x.Count).ToList();

        if (DashboardCategorySummaryItems != null)
        {
            DashboardCategorySummaryItems.ItemsSource = summaryItems;
        }
    }

    private static string GetAssetCategoryFromFileNode(FileNodeViewModel node)
    {
        if (node.IsSkeletalMesh == true) return "Skeletal Mesh";
        if (node.IsMaterial == true)     return "Material Instance";
        if (node.IsTexture == true)      return "Texture 2D";
        if (node.IsAnimation == true)    return "Animation Sequence";
        if (node.IsSkeleton == true)     return "Skeleton";
        if (node.IsPhysicsAsset == true) return "Physics Asset";
        if (node.IsAudioFile == true)    return "Audio & Sound";

        var name = (node.Name ?? string.Empty).ToLowerInvariant();
        var path = (node.FullPath ?? string.Empty).ToLowerInvariant();
        var ext = Path.GetExtension(name);

        if (ext == ".uexp" || ext == ".ubulk" || ext == ".uptnl") return "Unreal Bulk Exports";
        if (ext == ".utoc" || ext == ".ucas" || ext == ".pak") return "Archive Containers";
        if (ext == ".umap") return "Level Map";
        if (ext == ".locres") return "Localization";
        if (ext == ".ini" || ext == ".json") return "Config";

        if (name.StartsWith("sk_") || name.StartsWith("skm_") || path.Contains("skeletalmesh")) return "Skeletal Mesh";
        if (name.StartsWith("sm_") || path.Contains("staticmesh")) return "Static Mesh";
        if (name.StartsWith("mi_") || name.StartsWith("m_") || path.Contains("material")) return "Material Instance";
        if (name.StartsWith("t_") || name.StartsWith("tex_") || path.Contains("texture") || ext == ".dds" || ext == ".png") return "Texture 2D";
        if (name.StartsWith("phys_") || path.Contains("physicsasset")) return "Physics Asset";
        if (name.StartsWith("abp_") || name.StartsWith("am_") || path.Contains("anim")) return "Animation Sequence";
        if (name.StartsWith("s_") || path.Contains("skeleton")) return "Skeleton";
        if (name.StartsWith("wbp_") || name.StartsWith("ui_")) return "UI Widget";
        if (name.StartsWith("bp_") || path.Contains("blueprint")) return "Blueprint";
        if (name.StartsWith("da_") || path.Contains("dataasset")) return "Data Asset";
        if (name.StartsWith("dt_") || path.Contains("datatable")) return "Data Table";
        if (name.StartsWith("ns_") || name.StartsWith("ps_") || path.Contains("niagara") || path.Contains("particle")) return "Niagara VFX";
        if (path.Contains("sound") || path.Contains("audio") || path.Contains("wwise")) return "Audio & Sound";

        return "Unreal Asset";
    }

    private void CollectAllFiles(List<FileNodeViewModel> nodes, List<FileNodeViewModel> results)
    {
        foreach (var node in nodes)
        {
            if (!node.IsDirectory)
            {
                results.Add(node);
            }

            if (node.IsDirectory && node.Children.Count > 0)
            {
                CollectAllFiles(node.Children.ToList(), results);
            }
        }
    }
}
