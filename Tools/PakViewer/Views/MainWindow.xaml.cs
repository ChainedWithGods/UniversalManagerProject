using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using PakViewer.ViewModels;
using PakViewer.Models;
using PakViewer.Services;
using PakViewer.Helpers;
using PakViewer.Converters;
using PakViewer.Settings;
using Serilog;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using System.IO;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using CUE4Parse.Compression;
using CUE4Parse.UE4.Assets;
using Windows.System;

namespace PakViewer.Views;

public sealed partial class MainWindow : Window, INotifyPropertyChanged
{
    public event PropertyChangedEventHandler? PropertyChanged;

    private bool _isWindowActive = true;

    private void OnPropertyChanged([CallerMemberName] string? propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
    public MainViewModel ViewModel { get; }
    public BoolToVisibilityConverter BoolToVisConverter { get; } = new();
    public InverseBoolToVisibilityConverter InverseBoolToVisConverter { get; } = new();

    private int _currentTabIndex = 0;
    private readonly FilePreviewService _previewService;
    private readonly List<ImageViewerWindow> _imageWindows = new();
    private readonly List<MaterialViewerWindow> _materialWindows = new();
    private SettingsPanel? _settingsPanel;
    private const int MaxOpenViewerWindows = 10;
    private bool _isMinimalMode = false;
    private readonly Microsoft.UI.Dispatching.DispatcherQueueTimer? _memoryMonitorTimer;
    private const long HighMemoryThresholdMB = 700;
    private bool _isViewerOpen;
    private ConflictDashboardView DashboardControl = null!;

    private readonly ObservableCollection<AssetCategory> _assetCategories = new();

    public ObservableCollection<AssetCategory> AssetCategories => _assetCategories;

    private const string DefaultCategoryIconSource = "ms-appx:///Icons/Types.ico";

    private static string GetCategoryIconSource(string? categoryName) => categoryName switch
    {
        "Rig Blueprints" or "Animation Blueprints" or "Widget Blueprints" or "Blueprints" or "Enums" or "Structs" or "Metadata"
            => "ms-appx:///Icons/Blueprint.ico",

        "Static Meshes" or "Skeletal Meshes" or "Customizable Objects" or "Nanite Meshes"
            => "ms-appx:///Icons/Character.ico",

        "Textures" or "Images"
            => "ms-appx:///Icons/Textures.ico",

        "Materials" or "Material Functions" or "Material Parameters" or "Material Editor Data"
            => "ms-appx:///Icons/Material.ico",

        "Animations" or "Skeletons" or "Rigs" or "Particles" or "Physics Assets"
            => "ms-appx:///Icons/atom.ico",

        "Worlds/Levels" or "Build Data" or "Level Sequences" or "Foliage" or "Maps"
            => "ms-appx:///Icons/World.ico",

        "Item Definitions" or "Data Tables" or "Curves" or "Redirectors" or "Physical Materials" or "Asset Data" or "Bulk Data"
            => "ms-appx:///Icons/data.ico",

        "Audio Events" or "Sound Banks" or "Audio" or "Audio Files"
            => "ms-appx:///Icons/Music.ico",

        "Video" or "Fonts"
            => "ms-appx:///Icons/Music.ico",

        "UI Widgets"
            => "ms-appx:///Icons/UI.ico",

        "Configuration" or "JSON Data" or "Text Files" or "Shaders" or "Localization" or "Archives" or "Assets" or "Unknown"
            => "ms-appx:///Icons/Types.ico",

        _ => DefaultCategoryIconSource
    };

    public bool IsMinimalMode
    {
        get => _isMinimalMode;
        set
        {
            _isMinimalMode = value;
            ViewModel.IsMinimalMode = value;
            UpdateViewMode();
        }
    }

    public MainWindow(App.CommandLineArgs? cmdArgs = null)
    {
        this.InitializeComponent();
        ViewModel = new MainViewModel();
        ViewModel.SettingsRequested += (_, _) => ShowSettingsPanel();
        ViewModel.GameLoading += (_, _) =>
        {
            _previewService.ClearAllCache();
            ViewModel.GetPakLoader().ClearPackageCache();
        };
        _previewService = new FilePreviewService();
        TextPreview.AssetPathClicked += TextPreview_AssetPathClicked;
        App.ApplyThemeToWindow(this, AppSettings.Instance.Theme);
        ElementSoundPlayer.State = AppSettings.Instance.SystemSoundsEnabled ? ElementSoundPlayerState.On : ElementSoundPlayerState.Off;

        _memoryMonitorTimer = DispatcherQueue.CreateTimer();
        _memoryMonitorTimer.Interval = TimeSpan.FromSeconds(5);
        _memoryMonitorTimer.Tick += (_, _) => SmartMemoryCleanup();
        _memoryMonitorTimer.Start();

        if (cmdArgs?.MinimalUI == true)
        {
            _isMinimalMode = true;
            ViewModel.IsMinimalMode = true;
            Log.Information("Starting in Minimal Mode (Asset Gallery View)");
            UpdateViewMode();
        }

        ApplyShortcuts();

        DashboardControl = new ConflictDashboardView();
        DashboardContainer.Children.Add(DashboardControl);

        DashboardControl.View3DRequested += async (_, args) =>
        {
            if (args?.TargetNode != null)
            {
                await LoadFilePreview(args.TargetNode);
            }
            else if (!string.IsNullOrEmpty(args?.TargetPath))
            {
                var targetNode = FindNodeByAssetPath(args.TargetPath);
                if (targetNode != null)
                {
                    await LoadFilePreview(targetNode);
                }
                else if (_currentModFileTree != null && _currentModFileTree.Count > 0)
                {
                    await AutoOpenSkeletalMeshAsync(_currentModFileTree);
                }
            }
            else if (_currentModFileTree != null && _currentModFileTree.Count > 0)
            {
                await AutoOpenSkeletalMeshAsync(_currentModFileTree);
            }
        };
        DashboardControl.ExplorerRequested += (_, _) => ShowFullView();

        ViewModel.PropertyChanged += async (s, e) =>
        {
            if (e.PropertyName == nameof(ViewModel.SelectedPreviewNode))
            {
                Bindings.Update();

                var node = ViewModel.SelectedPreviewNode;
                if (node != null && !node.IsDirectory)
                {
                    await LoadFilePreview(node);
                }
                else
                {
                    TextPreview.Clear();
                    ModelViewerVisibility = Visibility.Collapsed;

                    SmartMemoryCleanup();
                }
            }
            else if (e.PropertyName == nameof(ViewModel.SelectedNode))
            {
                if (ViewModel.SelectedNode != null)
                {
                    await ScrollTreeToNodeAsync(ViewModel.SelectedNode);
                }
            }
        };

        Title = "PakViewer";

        var appWindow = this.AppWindow;
        appWindow.Resize(new Windows.Graphics.SizeInt32(1400, 900));

        var iconPath = System.IO.Path.Combine(AppContext.BaseDirectory, "Icons", "PakViewer.ico");
        if (System.IO.File.Exists(iconPath))
            appWindow.SetIcon(iconPath);

        appWindow.TitleBar.ExtendsContentIntoTitleBar = true;
        appWindow.TitleBar.ButtonBackgroundColor = Microsoft.UI.ColorHelper.FromArgb(0, 0, 0, 0);
        appWindow.TitleBar.ButtonInactiveBackgroundColor = Microsoft.UI.ColorHelper.FromArgb(0, 0, 0, 0);
        SetTitleBar(AppTitleBar);

        if (Content is FrameworkElement root)
        {
            root.ActualThemeChanged += (_, _) => UpdateTitleBarColors(isActive: _isWindowActive);
            root.PreviewKeyDown += Root_PreviewKeyDown;
        }

        Activated += (_, e) =>
        {
            _isWindowActive = e.WindowActivationState != WindowActivationState.Deactivated;
            UpdateTitleBarColors(isActive: _isWindowActive);
        };

        UpdateTitleBarColors(isActive: true);

        Log.Information("MainWindow initialized successfully");

        if (cmdArgs != null)
        {

            bool hasActivated = false;
            this.Activated += async (s, e) =>
            {

                if (hasActivated) return;
                hasActivated = true;

                if (cmdArgs.ConflictMode)
                {
                    _isConflictMode = true;
                    Log.Information("Conflict Mode active");

                    if (!string.IsNullOrEmpty(cmdArgs.ConflictJsonPath) && File.Exists(cmdArgs.ConflictJsonPath))
                    {
                        try
                        {
                            var jsonText = await File.ReadAllTextAsync(cmdArgs.ConflictJsonPath);
                            _conflictDashboardData = System.Text.Json.JsonSerializer.Deserialize(
                                jsonText, Models.ConflictDashboardJsonContext.Default.ConflictDashboardData);
                            Log.Information($"Loaded conflict dashboard data for mod: {_conflictDashboardData?.ModName}, conflicts count: {_conflictDashboardData?.Conflicts?.Count ?? 0}");
                        }
                        catch (Exception exJson)
                        {
                            Log.Error(exJson, "Failed to deserialize conflict JSON payload");
                        }
                    }
                }

                if (!string.IsNullOrEmpty(cmdArgs.PaksPath))
                {
                    try
                    {
                        Log.Information($"Auto-loading game with paks path: {cmdArgs.PaksPath}");

                        var normalizedPaksPath = App.NormalizePath(cmdArgs.PaksPath);
                        var selectedGame = AppSettings.Instance.Games
                            .FirstOrDefault(g => string.Equals(App.NormalizePath(g.PakPath), normalizedPaksPath, StringComparison.OrdinalIgnoreCase));

                        if (selectedGame != null)
                        {
                            AppSettings.Instance.LastSelectedGame = selectedGame.Name;
                            Log.Information($"Selected game: {selectedGame.Name}");

                            if (!string.IsNullOrEmpty(cmdArgs.FileToOpen) && File.Exists(cmdArgs.FileToOpen))
                            {
                                Log.Information("Using MOD-ONLY mode for asset viewer");
                                await LoadModOnlyAsync(cmdArgs.FileToOpen, selectedGame);
                            }
                            else
                            {

                                Log.Information("Loading full game (all paks)");
                                await ViewModel.LoadGameAsync();
                                Log.Information("Game loaded successfully");
                            }
                        }
                        else
                        {
                            Log.Warning($"No game configuration found for paks path: {cmdArgs.PaksPath}");
                        }
                    }
                    catch (Exception ex)
                    {
                        Log.Error(ex, "Failed to auto-load game");
                        var dialog = new ContentDialog
                        {
                            Title = "Auto-Load Error",
                            Content = $"Failed to auto-load game:\n{ex.Message}",
                            CloseButtonText = "OK",
                            XamlRoot = this.Content.XamlRoot
                        };
                        await dialog.ShowAsync();
                    }
                }
                else if (!string.IsNullOrEmpty(cmdArgs.FileToOpen))
                {

                    await OpenFileAsync(cmdArgs.FileToOpen);
                }
            };
        }

        bool updateCheckStarted = false;
        Activated += async (_, _) =>
        {
            if (updateCheckStarted) return;
            updateCheckStarted = true;

            await Task.Delay(2500);
            await CheckForUpdatesSilentlyAsync();
        };
    }

    private async Task CheckForUpdatesSilentlyAsync()
    {
        try
        {
            if (!AppSettings.Instance.CheckForUpdatesOnStartup)
                return;

            var updateService = new UpdateService();
            var result = await updateService.CheckForUpdatesAsync();

            if (result.HasUpdate && result.Manifest != null)
            {
                if (!string.IsNullOrEmpty(AppSettings.Instance.IgnoredUpdateVersion) &&
                    string.Equals(AppSettings.Instance.IgnoredUpdateVersion, result.Manifest.Version, StringComparison.OrdinalIgnoreCase))
                {
                    Log.Information("Update {Version} is available but ignored by user settings", result.Manifest.Version);
                    return;
                }

                if (this.Content?.XamlRoot != null)
                {
                    await UpdatePrompt.ShowAsync(this.Content.XamlRoot, result);
                }
            }
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "Silent background update check failed");
        }
    }

    private void UpdateTitleBarColors(bool isActive)
    {
        try
        {
            _ = AppWindow;
            _ = Content;
        }
        catch (System.Runtime.InteropServices.COMException)
        {
            return;
        }

        var appWindow = AppWindow;

        var theme = (Content as FrameworkElement)?.ActualTheme ?? ElementTheme.Default;
        var isDark = theme == ElementTheme.Dark;

        var activeForeground = isDark ? Microsoft.UI.Colors.White : Microsoft.UI.Colors.Black;
        var inactiveForeground = isDark
            ? Microsoft.UI.ColorHelper.FromArgb(255, 180, 180, 180)
            : Microsoft.UI.ColorHelper.FromArgb(255, 120, 120, 120);

        var hoverBg = isDark
            ? Microsoft.UI.ColorHelper.FromArgb(25, 255, 255, 255)
            : Microsoft.UI.ColorHelper.FromArgb(20, 0, 0, 0);
        var pressedBg = isDark
            ? Microsoft.UI.ColorHelper.FromArgb(40, 255, 255, 255)
            : Microsoft.UI.ColorHelper.FromArgb(30, 0, 0, 0);

        appWindow.TitleBar.ButtonForegroundColor = isActive ? activeForeground : inactiveForeground;
        appWindow.TitleBar.ButtonHoverForegroundColor = activeForeground;
        appWindow.TitleBar.ButtonPressedForegroundColor = activeForeground;
        appWindow.TitleBar.ButtonInactiveForegroundColor = inactiveForeground;
        appWindow.TitleBar.ButtonHoverBackgroundColor = hoverBg;
        appWindow.TitleBar.ButtonPressedBackgroundColor = pressedBg;
    }

    private void Root_PreviewKeyDown(object sender, KeyRoutedEventArgs e)
    {
        if (e.Key == VirtualKey.Escape && SettingsOverlay.Visibility == Visibility.Visible)
        {
            HideSettingsPanel();
            e.Handled = true;
        }
    }

    private void ShowSettingsPanel()
    {
        if (SettingsOverlay.Visibility == Visibility.Visible)
        {
            _settingsPanel?.FocusInitialElement();
            return;
        }

        _settingsPanel = new SettingsPanel(_isMinimalMode);
        _settingsPanel.CloseRequested += SettingsPanel_CloseRequested;
        SettingsPanelHost.Content = _settingsPanel;
        SettingsOverlay.Visibility = Visibility.Visible;
        _settingsPanel.FocusInitialElement();
    }

    private void HideSettingsPanel()
    {
        if (_settingsPanel != null)
        {
            _settingsPanel.CloseRequested -= SettingsPanel_CloseRequested;
            _settingsPanel = null;
        }

        SettingsPanelHost.Content = null;
        SettingsOverlay.Visibility = Visibility.Collapsed;
        ApplyShortcuts();
        Bindings.Update();
    }

    private void SettingsPanel_CloseRequested(object? sender, EventArgs e)
    {
        HideSettingsPanel();
    }

    private void SettingsBackdrop_Click(object sender, RoutedEventArgs e)
    {
        HideSettingsPanel();
    }

    private void SmartMemoryCleanup()
    {
        var currentMB = GetCurrentMemoryMB();

        if (currentMB < HighMemoryThresholdMB)
            return;

        if (_isViewerOpen)
        {
            Log.Debug($"High memory ({currentMB}MB) but viewer is open, skipping aggressive cleanup");
            return;
        }

        Log.Information($"High memory detected: {currentMB}MB (threshold: {HighMemoryThresholdMB}MB), performing aggressive cleanup");

        _previewService.ClearAllCache();

        ViewModel.GetPakLoader().ClearPackageCache();

        GC.Collect(GC.MaxGeneration, GCCollectionMode.Aggressive, true, true);
        GC.WaitForPendingFinalizers();
        GC.Collect(GC.MaxGeneration, GCCollectionMode.Aggressive, true, true);

        var afterMB = GetCurrentMemoryMB();
        Log.Information($"Memory cleanup complete: {currentMB}MB -> {afterMB}MB (freed {currentMB - afterMB}MB)");
    }

    private static long GetCurrentMemoryMB()
    {
        using var proc = System.Diagnostics.Process.GetCurrentProcess();
        return proc.PrivateMemorySize64 / (1024 * 1024);
    }

    private async Task OpenFileAsync(string filePath)
    {
        try
        {
            if (File.Exists(filePath))
            {
                await ViewModel.LoadPakFileAsync(filePath);
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to open file");
            var dialog = new ContentDialog
            {
                Title = "Error Opening File",
                Content = $"Failed to open file:\n{ex.Message}",
                CloseButtonText = "OK",
                XamlRoot = this.Content.XamlRoot
            };
            await dialog.ShowAsync();
        }
    }

    private async Task LoadModOnlyAsync(string modFilePath, GameConfig gameConfig)
    {
        ViewModel.IsLoading = true;
        ViewModel.StatusText = $"Loading mod {Path.GetFileNameWithoutExtension(modFilePath)}...";

        try
        {
            Log.Information($"Loading mod only: {modFilePath}");

            var fileTree = new List<FileNodeViewModel>();
            var pakLoader = ViewModel.GetPakLoader();

            await Task.Run(() =>
            {
                pakLoader.LoadModOnly(modFilePath, gameConfig);
                fileTree = pakLoader.GetModFileTree(modFilePath);
            });

            ViewModel.SetFileTree(fileTree!);

            Log.Information($"Mod loaded successfully: {fileTree!.Count} root items");

            if (_isConflictMode)
            {
                PopulateDashboard(modFilePath, fileTree!);
                ShowDashboardView();
            }
            else
            {

                SwitchToFoldersTab();

                if (_isMinimalMode)
                {
                    PopulateMinimalModeCollections(fileTree!);
                    UpdateViewMode();
                }
            }

            ViewModel.StatusText = $"Loaded mod: {Path.GetFileNameWithoutExtension(modFilePath)}";
            ViewModel.IsLoading = false;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to load mod");
            ViewModel.StatusText = $"Error loading mod: {ex.Message}";
            var dialog = new ContentDialog
            {
                Title = "Error Loading Mod",
                Content = $"Failed to load mod:\n{ex.Message}",
                CloseButtonText = "OK",
                XamlRoot = this.Content.XamlRoot
            };
            await dialog.ShowAsync();
        }
        finally
        {
            ViewModel.IsLoading = false;
        }
    }

    private async Task DetectAllFileTypesAsync(List<FileNodeViewModel> fileTree)
    {
        try
        {

            var uassetFiles = new List<FileNodeViewModel>();
            CollectUassetFiles(fileTree, uassetFiles);

            if (uassetFiles.Count == 0)
            {
                return;
            }

            Log.Information($"Detecting file types for {uassetFiles.Count} .uasset files...");

            var provider = ViewModel.GetFileProvider();
            if (provider == null)
            {
                return;
            }

            var dispatcher = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();

            await Task.Run(async () =>
            {
                foreach (var file in uassetFiles)
                {
                    try
                    {
                        var package = provider.LoadPackage(file.FullPath);

                        bool isSkeletalMesh = false;
                        bool isAnimation = false;
                        bool isTexture = false;
                        bool isMaterial = false;
                        bool isPlayableAudio = false;

                        foreach (var exportLazy in package.ExportsLazy)
                        {
                            var obj = exportLazy?.Value;
                            if (obj == null) continue;

                            var typeName = obj.GetType().Name;

                            if (typeName.Contains("SoundWave", StringComparison.OrdinalIgnoreCase) ||
                                typeName.Contains("AkMediaAssetData", StringComparison.OrdinalIgnoreCase) ||
                                typeName.Contains("AkMediaAsset", StringComparison.OrdinalIgnoreCase) ||
                                typeName.Contains("ExternalSource", StringComparison.OrdinalIgnoreCase))
                            {
                                isPlayableAudio = true;
                            }

                            if (obj is CUE4Parse.UE4.Assets.Exports.SkeletalMesh.USkeletalMesh)
                            {
                                isSkeletalMesh = true;
                                break;
                            }
                            else if (obj is CUE4Parse.UE4.Assets.Exports.StaticMesh.UStaticMesh)
                            {
                                isSkeletalMesh = true;
                                break;
                            }
                            else if (obj is CUE4Parse.UE4.Assets.Exports.Texture.UTexture ||
                                     obj is CUE4Parse.UE4.Assets.Exports.Texture.UTexture2D)
                            {
                                isTexture = true;
                                break;
                            }
                            else if (obj is CUE4Parse.UE4.Assets.Exports.Material.UMaterial ||
                                     obj is CUE4Parse.UE4.Assets.Exports.Material.UMaterialInstance ||
                                     obj is CUE4Parse.UE4.Assets.Exports.Material.UMaterialInstanceConstant)
                            {
                                isMaterial = true;
                                break;
                            }
                            else if (obj is CUE4Parse.UE4.Assets.Exports.Animation.UAnimationAsset)
                            {
                                isAnimation = true;
                                break;
                            }
                        }

                        var capturedFile = file;
                        var capturedIsSkeletalMesh = isSkeletalMesh;
                        var capturedIsAnimation = isAnimation;
                        var capturedIsTexture = isTexture;
                        var capturedIsMaterial = isMaterial;
                        var capturedIsPlayableAudio = isPlayableAudio;

                        dispatcher.TryEnqueue(() =>
                        {
                            if (capturedIsSkeletalMesh)
                                capturedFile.IsSkeletalMesh = true;
                            else if (capturedIsAnimation)
                                capturedFile.IsAnimation = true;
                            else if (capturedIsTexture)
                                capturedFile.IsTexture = true;
                            else if (capturedIsMaterial)
                                capturedFile.IsMaterial = true;

                            if (capturedIsPlayableAudio)
                                capturedFile.IsAudioFile = true;
                        });
                    }
                    catch
                    {

                    }
                }
            });

            Log.Information("File type detection completed");

            ViewModel.GetPakLoader().ClearPackageCache();
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to detect file types");
        }
    }

    private async Task AutoOpenSkeletalMeshAsync(List<FileNodeViewModel> fileTree)
    {
        try
        {

            var uassetFiles = new List<FileNodeViewModel>();
            CollectUassetFiles(fileTree, uassetFiles);

            if (uassetFiles.Count == 0)
            {
                Log.Information("No .uasset files found in mod");
                return;
            }

            Log.Information($"Found {uassetFiles.Count} .uasset files, searching for skeletal mesh...");

            var meshCandidates = new List<FileNodeViewModel>();

            foreach (var f in uassetFiles)
            {
                if (f.Name.StartsWith("SK_", StringComparison.OrdinalIgnoreCase) ||
                    f.Name.StartsWith("SKM_", StringComparison.OrdinalIgnoreCase) ||
                    f.Name.Contains("SkeletalMesh", StringComparison.OrdinalIgnoreCase))
                {
                    meshCandidates.Add(f);
                }
            }

            var provider = ViewModel.GetFileProvider();
            if (provider != null)
            {
                foreach (var candidate in uassetFiles)
                {
                    if (meshCandidates.Contains(candidate)) continue;
                    try
                    {
                        var package = provider.LoadPackage(candidate.FullPath);
                        foreach (var exportLazy in package.ExportsLazy)
                        {
                            var obj = exportLazy?.Value;
                            if (obj is CUE4Parse.UE4.Assets.Exports.SkeletalMesh.USkeletalMesh ||
                                obj is CUE4Parse.UE4.Assets.Exports.StaticMesh.UStaticMesh)
                            {
                                meshCandidates.Add(candidate);
                                break;
                            }
                        }
                    }
                    catch
                    {

                    }
                }
            }

            FileNodeViewModel? meshToOpen = meshCandidates.FirstOrDefault(f =>
                !f.Name.Contains("Placeholder", StringComparison.OrdinalIgnoreCase))
                ?? meshCandidates.FirstOrDefault();

            if (meshToOpen != null)
            {
                Log.Information($"Auto-opening skeletal mesh: {meshToOpen.Name}");

                ExpandToNode(meshToOpen);
                ViewModel.SelectedPreviewNode = meshToOpen;

                provider = ViewModel.GetFileProvider();
                if (provider != null)
                {
                    try
                    {
                        var result = await _previewService.LoadFileAsync(provider, meshToOpen.FullPath);

                        if (result.AssetCategory == EAssetCategory.SkeletalMesh)
                        {
                            meshToOpen.IsSkeletalMesh = true;
                        }
                        else if (result.AssetCategory == EAssetCategory.StaticMesh)
                        {

                            meshToOpen.IsSkeletalMesh = true;
                        }

                        if (result.IsModel || result.AssetCategory == EAssetCategory.StaticMesh || result.AssetCategory == EAssetCategory.SkeletalMesh)
                        {
                            Log.Information($"Auto-launching 3D viewer for: {meshToOpen.Name}");
                            ModelViewerService.LaunchPakViewer(provider, meshToOpen.FullPath, result.ModelName ?? meshToOpen.Name);
                        }
                    }
                    catch (Exception ex)
                    {
                        Log.Error(ex, "Failed to auto-launch 3D viewer");
                    }
                }
            }
            else
            {
                Log.Information("No skeletal mesh detected in mod");
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to auto-open skeletal mesh");
        }
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

    private void CollectUassetFiles(List<FileNodeViewModel> nodes, List<FileNodeViewModel> results)
    {
        foreach (var node in nodes)
        {
            if (!node.IsDirectory && node.Extension == "uasset")
            {
                results.Add(node);
            }

            if (node.IsDirectory && node.Children.Count > 0)
            {
                CollectUassetFiles(node.Children.ToList(), results);
            }
        }
    }

    private void ExpandToNode(FileNodeViewModel node)
    {

        var parent = node.Parent;
        while (parent != null)
        {
            parent.IsExpanded = true;
            parent = parent.Parent;
        }
    }

    private void SwitchToFoldersTab()
    {

        _currentTabIndex = 1;
        ArchivesTab.Visibility = Visibility.Collapsed;
        FoldersTab.Visibility = Visibility.Visible;
        PackagesTab.Visibility = Visibility.Collapsed;
    }

    private void UpdateViewMode()
    {
        if (FullViewGrid != null && MinimalViewGrid != null)
        {
            FullViewGrid.Visibility = _isMinimalMode ? Visibility.Collapsed : Visibility.Visible;
            MinimalViewGrid.Visibility = _isMinimalMode ? Visibility.Visible : Visibility.Collapsed;
        }

        if (GameSelectorCombo != null)
            GameSelectorCombo.Visibility = _isMinimalMode ? Visibility.Collapsed : Visibility.Visible;
        if (GameNameText != null)
            GameNameText.Visibility = _isMinimalMode ? Visibility.Visible : Visibility.Collapsed;
        if (LoadGameButton != null)
            LoadGameButton.Visibility = _isMinimalMode ? Visibility.Collapsed : Visibility.Visible;
    }

    private CancellationTokenSource? _thumbnailLoadCts;

    private async void ShowThumbnailsToggle_Click(object sender, RoutedEventArgs e)
    {
        if (ShowThumbnailsToggle.IsChecked == true)
        {
            await LoadMinimalThumbnailsAsync();
        }
        else
        {

            _thumbnailLoadCts?.Cancel();

            foreach (var category in _assetCategories)
            {
                foreach (var asset in category.Assets)
                {
                    if (asset.Thumbnail != null)
                    {
                        asset.Thumbnail = null;
                    }
                }
            }
        }
    }

    private async Task LoadMinimalThumbnailsAsync()
    {
        _thumbnailLoadCts?.Cancel();
        _thumbnailLoadCts = new CancellationTokenSource();
        var token = _thumbnailLoadCts.Token;

        var provider = ViewModel.GetFileProvider();
        if (provider == null) return;

        var dispatcher = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();

        var textureAssets = _assetCategories
            .SelectMany(c => c.Assets)
            .Where(a => a.IsTexture)
            .ToList();

        if (textureAssets.Count == 0) return;

        Log.Information($"Loading thumbnails for {textureAssets.Count} texture assets in minimal mode");

        var batchSize = 8;
        for (int i = 0; i < textureAssets.Count; i += batchSize)
        {
            if (token.IsCancellationRequested) break;

            var batch = textureAssets.Skip(i).Take(batchSize);
            var tasks = batch.Select(async asset =>
            {
                if (token.IsCancellationRequested) return;
                if (asset.Thumbnail != null) return;

                try
                {
                    var thumb = await TextureLoader.LoadThumbnailAsync(provider, asset.FullPath, dispatcher);
                    if (thumb != null && !token.IsCancellationRequested)
                    {
                        dispatcher.TryEnqueue(() =>
                        {
                            if (!token.IsCancellationRequested)
                            {
                                asset.Thumbnail = thumb;
                            }
                        });
                    }
                }
                catch (Exception ex)
                {
                    Log.Warning(ex, $"Failed to load thumbnail for {asset.Name}");
                }
            });

            await Task.WhenAll(tasks);
            try
            {
                await Task.Delay(10, token);
            }
            catch (OperationCanceledException)
            {
                break;
            }
        }

        Log.Information("Thumbnail loading complete for minimal mode");
    }

    private void PopulateMinimalModeCollections(List<FileNodeViewModel> fileTree)
    {

        _assetCategories.Clear();

        var allFiles = new List<FileNodeViewModel>();
        CollectAllFiles(fileTree, allFiles);

        Log.Information($"Smart categorizing {allFiles.Count} files for Minimal Mode");

        var categoryMap = new Dictionary<string, AssetCategory>();

        var provider = ViewModel.GetFileProvider();

        foreach (var file in allFiles)
        {
            string? categoryName = null;
            string categoryIcon = DefaultCategoryIconSource;

            try
            {

                if (file.Extension == "uasset" && provider != null)
                {
                    try
                    {
                        var package = provider.LoadPackage(file.FullPath);

                        foreach (var exportLazy in package.ExportsLazy)
                        {
                            var obj = exportLazy?.Value;
                            if (obj == null) continue;

                            var typeName = obj.GetType().Name;

                            if (typeName.Contains("RigVMBlueprintGeneratedClass"))
                            {
                                categoryName = "Rig Blueprints";
                                categoryIcon = "📘";
                            }
                            else if (typeName.Contains("AnimBlueprintGeneratedClass"))
                            {
                                categoryName = "Animation Blueprints";
                                categoryIcon = "🎬";
                            }
                            else if (typeName.Contains("WidgetBlueprintGeneratedClass"))
                            {
                                categoryName = "Widget Blueprints";
                                categoryIcon = "🖥️";
                            }
                            else if (typeName.Contains("BlueprintGeneratedClass") || typeName == "UFunction")
                            {
                                categoryName = "Blueprints";
                                categoryIcon = "📘";
                            }
                            else if (typeName == "UUserDefinedEnum")
                            {
                                categoryName = "Enums";
                                categoryIcon = "🔢";
                            }
                            else if (typeName == "UUserDefinedStruct")
                            {
                                categoryName = "Structs";
                                categoryIcon = "🏗️";
                            }
                            else if (typeName.Contains("Blueprint"))
                            {
                                categoryName = "Blueprints";
                                categoryIcon = "📘";
                            }
                            else if (typeName.Contains("CookedMetaData"))
                            {
                                categoryName = "Metadata";
                                categoryIcon = "";
                            }

                            else if (typeName.Contains("StaticMesh"))
                            {
                                categoryName = "Static Meshes";
                                categoryIcon = "";
                                file.IsSkeletalMesh = true;
                            }
                            else if (typeName.Contains("SkeletalMesh"))
                            {
                                categoryName = "Skeletal Meshes";
                                categoryIcon = "";
                                file.IsSkeletalMesh = true;
                            }
                            else if (typeName.Contains("CustomizableObject"))
                            {
                                categoryName = "Customizable Objects";
                                categoryIcon = "";
                            }
                            else if (typeName.Contains("NaniteDisplacedMesh"))
                            {
                                categoryName = "Nanite Meshes";
                                categoryIcon = "";
                            }

                            else if (typeName.Contains("Texture"))
                            {
                                categoryName = "Textures";
                                categoryIcon = "";
                                file.IsTexture = true;
                            }

                            else if (typeName.Contains("MaterialInterface") || typeName.Contains("Material"))
                            {
                                if (typeName.Contains("EditorOnlyData") || typeName.Contains("EditorData"))
                                {
                                    categoryName = "Material Editor Data";
                                    categoryIcon = "";
                                }
                                else if (typeName.Contains("MaterialFunction"))
                                {
                                    categoryName = "Material Functions";
                                    categoryIcon = "";
                                }
                                else if (typeName.Contains("MaterialParameterCollection"))
                                {
                                    categoryName = "Material Parameters";
                                    categoryIcon = "";
                                }
                                else
                                {
                                    categoryName = "Materials";
                                    categoryIcon = "";
                                    file.IsMaterial = true;
                                }
                            }

                            else if (typeName.Contains("AnimationAsset") || typeName.Contains("Anim"))
                            {
                                categoryName = "Animations";
                                categoryIcon = "";
                            }
                            else if (typeName.Contains("Skeleton"))
                            {
                                categoryName = "Skeletons";
                                categoryIcon = "";
                            }
                            else if (typeName.Contains("Rig"))
                            {
                                categoryName = "Rigs";
                                categoryIcon = "";
                            }

                            else if (typeName.Contains("World"))
                            {
                                categoryName = "Worlds/Levels";
                                categoryIcon = "";
                            }
                            else if (typeName.Contains("MapBuildDataRegistry"))
                            {
                                categoryName = "Build Data";
                                categoryIcon = "";
                            }
                            else if (typeName.Contains("LevelSequence"))
                            {
                                categoryName = "Level Sequences";
                                categoryIcon = "";
                            }
                            else if (typeName.Contains("FoliageType"))
                            {
                                categoryName = "Foliage";
                                categoryIcon = "";
                            }

                            else if (typeName.Contains("ItemDefinition"))
                            {
                                categoryName = "Item Definitions";
                                categoryIcon = "";
                            }
                            else if (typeName.Contains("DataAsset") || typeName.Contains("DataTable") || typeName.Contains("CurveTable") || typeName.Contains("StringTable"))
                            {
                                categoryName = "Data Tables";
                                categoryIcon = "";
                            }
                            else if (typeName.Contains("Curve"))
                            {
                                categoryName = "Curves";
                                categoryIcon = "";
                            }
                            else if (typeName.Contains("PhysicsAsset"))
                            {
                                categoryName = "Physics Assets";
                                categoryIcon = "";
                            }
                            else if (typeName.Contains("ObjectRedirector"))
                            {
                                categoryName = "Redirectors";
                                categoryIcon = "";
                            }
                            else if (typeName.Contains("PhysicalMaterial"))
                            {
                                categoryName = "Physical Materials";
                                categoryIcon = "";
                            }

                            else if (typeName.Contains("SoundAtomCue") || typeName.Contains("AkAudioEvent") || typeName.Contains("SoundCue") ||
                                     typeName.Contains("FMODEvent") || typeName.Contains("AkAssetData") || typeName.Contains("AkAssetPlatformData"))
                            {
                                categoryName = "Audio Events";
                                categoryIcon = "";
                            }
                            else if (typeName.Contains("FMODBank") || typeName.Contains("AkAudioBank") || typeName.Contains("AtomWaveBank") ||
                                     typeName.Contains("AkInitBank") || typeName.Contains("SoundBank"))
                            {
                                categoryName = "Sound Banks";
                                categoryIcon = "";
                            }
                            else if (typeName.Contains("Sound") || typeName.Contains("Audio") || typeName.Contains("Ak") ||
                                     typeName.Contains("FMOD") || typeName.Contains("Wwise"))
                            {
                                categoryName = "Audio";
                                categoryIcon = "";

                                if (typeName.Contains("SoundWave", StringComparison.OrdinalIgnoreCase) ||
                                    typeName.Contains("AkMediaAssetData", StringComparison.OrdinalIgnoreCase) ||
                                    typeName.Contains("AkMediaAsset", StringComparison.OrdinalIgnoreCase) ||
                                    typeName.Contains("ExternalSource", StringComparison.OrdinalIgnoreCase))
                                {
                                    file.IsAudioFile = true;
                                }
                            }

                            else if (typeName.Contains("FileMediaSource") || typeName.Contains("Video"))
                            {
                                categoryName = "Video";
                                categoryIcon = "";
                            }
                            else if (typeName.Contains("Font") || typeName.Contains("FontFace"))
                            {
                                categoryName = "Fonts";
                                categoryIcon = "";
                            }

                            else if (typeName.Contains("Niagara") || typeName.Contains("ParticleSystem"))
                            {
                                categoryName = "Particles";
                                categoryIcon = "";
                            }

                            else if (typeName.Contains("Widget") || typeName.Contains("UMG"))
                            {
                                categoryName = "UI Widgets";
                                categoryIcon = "";
                            }

                            if (!string.IsNullOrEmpty(categoryName))
                                break;
                        }
                    }
                    catch
                    {

                    }
                }

                if (string.IsNullOrEmpty(categoryName))
                {
                    switch (file.Extension.ToLower())
                    {
                        case "uasset":
                            categoryName = "Assets";
                            categoryIcon = "";
                            break;
                        case "uexp":
                            categoryName = "Asset Data";
                            categoryIcon = "";
                            break;
                        case "ubulk":
                            categoryName = "Bulk Data";
                            categoryIcon = "";
                            break;
                        case "umap":
                            categoryName = "Maps";
                            categoryIcon = "";
                            break;
                        case "ini":
                            categoryName = "Configuration";
                            categoryIcon = "";
                            break;
                        case "json":
                            categoryName = "JSON Data";
                            categoryIcon = "";
                            break;
                        case "txt":
                        case "log":
                            categoryName = "Text Files";
                            categoryIcon = "";
                            break;
                        case "png":
                        case "jpg":
                        case "jpeg":
                        case "bmp":
                        case "tga":
                        case "dds":
                            categoryName = "Images";
                            categoryIcon = "";
                            break;
                        case "wav":
                        case "mp3":
                        case "ogg":
                        case "flac":
                            categoryName = "Audio Files";
                            categoryIcon = "";
                            break;
                        case "pak":
                        case "ucas":
                        case "utoc":
                            categoryName = "Archives";
                            categoryIcon = "";
                            break;
                        case "ushaderbytecode":
                        case "ushadercode":
                            categoryName = "Shaders";
                            categoryIcon = "";
                            break;
                        case "locmeta":
                        case "locres":
                            categoryName = "Localization";
                            categoryIcon = "";
                            break;
                        default:
                            categoryName = file.Extension.ToUpper() + " Files";
                            categoryIcon = "";
                            break;
                    }
                }

                if (!string.IsNullOrEmpty(categoryName))
                {
                    categoryIcon = GetCategoryIconSource(categoryName);
                }

                if (!categoryMap.ContainsKey(categoryName))
                {
                    categoryMap[categoryName] = new AssetCategory(categoryName, categoryIcon);
                }

                categoryMap[categoryName].Assets.Add(file);
            }
            catch (Exception ex)
            {
                Log.Warning(ex, $"Failed to categorize file: {file.Name}");

                if (!categoryMap.ContainsKey("Unknown"))
                {
                    categoryMap["Unknown"] = new AssetCategory("Unknown", GetCategoryIconSource("Unknown"));
                }
                categoryMap["Unknown"].Assets.Add(file);
            }
        }

        var sortedCategories = categoryMap.Values
            .OrderByDescending(c => c.Assets.Count)
            .ToList();

        foreach (var category in sortedCategories)
        {
            category.Count = category.Assets.Count;
            _assetCategories.Add(category);
            Log.Information($"  {category.Name}: {category.Count} items");
        }

        Log.Information($"Smart categorization complete: {_assetCategories.Count} categories detected");

        ViewModel.GetPakLoader().ClearPackageCache();

        OnPropertyChanged(nameof(AssetCategories));

        Bindings.Update();
    }

    private async void MinimalAsset_DoubleTapped(object sender, DoubleTappedRoutedEventArgs e)
    {
        if (sender is FrameworkElement element && element.DataContext is FileNodeViewModel node)
        {
            try
            {
                var provider = ViewModel.GetFileProvider();
                if (provider == null)
                {
                    Log.Warning("No file provider available");
                    return;
                }

                if (CodeViewToggle != null && CodeViewToggle.IsChecked == true)
                {
                    var result = await _previewService.LoadFileAsync(provider, node.FullPath);

                    if (result.IsText && !string.IsNullOrEmpty(result.TextContent))
                    {
                        var extension = Path.GetExtension(node.Name).ToLower();
                        var viewer = new CodeViewerWindow();
                        await viewer.LoadContentAsync(
                            result.TextContent,
                            extension,
                            node.Name,
                            node.FullPath,
                            node.Size);
                        viewer.Activate();
                    }
                    else
                    {
                        Log.Information($"No text content available for code view: {node.Name}");
                    }
                    return;
                }

                var previewResult = await _previewService.LoadFileAsync(provider, node.FullPath);

                if (previewResult.IsAudio)
                {
                    OpenAudioPlayer(node, previewResult);
                }
                else if (node.IsTexture || previewResult.IsTexture)
                {
                    await OpenImageViewerAsync(node);
                }
                else if (node.IsMaterial || previewResult.MaterialData != null)
                {
                    OpenMaterialViewer(node, previewResult);
                }
                else if (node.IsSkeletalMesh || previewResult.IsModel)
                {
                    _isViewerOpen = true;
                    ModelViewerService.LaunchPakViewer(provider, node.FullPath, previewResult.ModelName ?? node.Name);
                    _isViewerOpen = false;

                    _previewService.ClearCacheFor(node.FullPath);
                    ViewModel.GetPakLoader().ClearPackageCache();
                }
            }
            catch (Exception ex)
            {
                Log.Error(ex, $"Failed to open asset: {node.Name}");
            }
        }
    }

    public void ApplyShortcuts()
    {

        if (Content is Grid rootGrid)
        {
            rootGrid.KeyboardAccelerators.Clear();

            foreach (var shortcut in AppSettings.Instance.Shortcuts)
            {
                var accelerator = new KeyboardAccelerator
                {
                    Key = shortcut.Key,
                    Modifiers = shortcut.Modifiers
                };

                switch (shortcut.Id)
                {
                    case "LoadGame":
                        accelerator.Invoked += LoadGameAccelerator_Invoked;
                        break;
                    case "Search":
                        accelerator.Invoked += SearchAccelerator_Invoked;
                        break;
                    case "Settings":
                        accelerator.Invoked += SettingsAccelerator_Invoked;
                        break;
                }

                rootGrid.KeyboardAccelerators.Add(accelerator);
            }
        }
    }

    private async Task LoadFilePreview(FileNodeViewModel? node)
    {

        ModelViewerVisibility = Visibility.Collapsed;
        _currentModelName = null;
        Bindings.Update();

        if (node == null || node.IsDirectory)
        {
            TextPreview.Clear();
            ModelViewerVisibility = Visibility.Collapsed;
            Bindings.Update();
            return;
        }

        try
        {
            Log.Information($"Loading file preview: {node.Name}");

            var provider = ViewModel.GetFileProvider();
            if (provider == null)
            {
                TextPreview.Clear();
                return;
            }

            var result = await _previewService.LoadFileAsync(provider, node.FullPath);

            if (result.IsModel)
            {
                ModelViewerVisibility = Visibility.Visible;
                _currentModelName = result.ModelName;
            }
            else
            {
                ModelViewerVisibility = Visibility.Collapsed;
                _currentModelName = null;
            }
            Bindings.Update();

            if (result.IsText && !string.IsNullOrEmpty(result.TextContent))
            {
                var extension = Path.GetExtension(node.Name).ToLower();
                await TextPreview.LoadContentAsync(
                    result.TextContent,
                    extension,
                    node.Name,
                    node.FullPath,
                    node.Size);
                Log.Information($"✓ Loaded text content with syntax highlighting: {node.Name}");
            }
            else
            {
                TextPreview.Clear();
            }
        }
        catch (System.Exception ex)
        {
            TextPreview.Clear();
            Log.Error(ex, $"Failed to load preview for {node.Name}");
        }
    }

    private void NavigateBack_Click(object sender, RoutedEventArgs e)
    {
        ElementSoundPlayer.Play(ElementSoundKind.MovePrevious);
        ViewModel.NavigateBack();
    }

    private void NavigateForward_Click(object sender, RoutedEventArgs e)
    {
        ElementSoundPlayer.Play(ElementSoundKind.MoveNext);
        ViewModel.NavigateForward();
    }

    private void TabButton_Click(object sender, RoutedEventArgs e)
    {
        if (sender is Button button && button.Tag is string tagStr)
        {
            int tabIndex = int.Parse(tagStr);
            SwitchToTab(tabIndex);
        }
    }

    private void SwitchToTab(int index)
    {
        _currentTabIndex = index;

        ArchivesTab.Visibility = Visibility.Collapsed;
        FoldersTab.Visibility = Visibility.Collapsed;
        PackagesTab.Visibility = Visibility.Collapsed;

        switch (index)
        {
            case 0: ArchivesTab.Visibility = Visibility.Visible; break;
            case 1: FoldersTab.Visibility = Visibility.Visible; break;
            case 2: PackagesTab.Visibility = Visibility.Visible; break;
        }

        TabIndicator.Margin = new Thickness(index * 126, 0, 254 - (index * 126), 0);
    }

    private void LoadGameAccelerator_Invoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args)
    {
        if (ViewModel.LoadGameCommand.CanExecute(null))
        {
            ViewModel.LoadGameCommand.Execute(null);
        }
        args.Handled = true;
    }

    private void SearchAccelerator_Invoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args)
    {
        if (ArchivesTab.Visibility == Visibility.Visible)
        {
            ArchivesSearchBox.Focus(FocusState.Keyboard);
        }
        else if (FoldersTab.Visibility == Visibility.Visible)
        {
            FoldersSearchBox.Focus(FocusState.Keyboard);
        }
        args.Handled = true;
    }

    private void SettingsAccelerator_Invoked(KeyboardAccelerator sender, KeyboardAcceleratorInvokedEventArgs args)
    {
        if (ViewModel.OpenSettingsCommand.CanExecute(null))
        {
            ViewModel.OpenSettingsCommand.Execute(null);
        }
        args.Handled = true;
    }

    private async void LoadArchives_Click(object sender, RoutedEventArgs e)
    {
        switch (ViewModel.SelectedLoadingMode)
        {
            case LoadingMode.All:
                SwitchToTab(1);
                await ViewModel.LoadAllArchivesAsync();
                break;
            case LoadingMode.Multiple:
            default:
                if (ArchivesListView.SelectedItems.Count > 0)
                {
                    var selectedArchives = new List<ArchiveViewModel>();
                    foreach (var item in ArchivesListView.SelectedItems)
                    {
                        if (item is ArchiveViewModel archive)
                        {
                            selectedArchives.Add(archive);
                        }
                    }
                    SwitchToTab(1);
                    await ViewModel.LoadFoldersForArchivesAsync(selectedArchives);
                }
                else
                {
                    Log.Warning("No archives selected");
                }
                break;
        }
    }

    private void ArchivesList_DoubleTapped(object sender, DoubleTappedRoutedEventArgs e)
    {
        LoadArchives_Click(sender, null!);
    }

    private void FoldersTreeView_ItemInvoked(TreeView sender, TreeViewItemInvokedEventArgs args)
    {
        if (args.InvokedItem is FileNodeViewModel node)
        {
            ViewModel.SelectedNode = node;
        }
    }

    private async void FoldersTreeView_DoubleTapped(object sender, DoubleTappedRoutedEventArgs e)
    {
        if (e.OriginalSource is FrameworkElement element && element.DataContext is FileNodeViewModel node)
        {
            if (node.IsDirectory)
            {
                return;
            }

            try
            {
                var provider = ViewModel.GetFileProvider();
                if (provider == null)
                {
                    Log.Warning("No file provider available");
                    return;
                }

                if (node.IsTexture)
                {
                    await OpenImageViewerAsync(node);
                    return;
                }

                var result = await _previewService.LoadFileAsync(provider, node.FullPath);

                if (result.IsTexture || result.AssetCategory == EAssetCategory.Texture)
                {
                    Log.Information("Fallback: Detected texture from asset content. Opening image viewer.");

                    node.IsTexture = true;
                    await OpenImageViewerAsync(node);
                    return;
                }

                if (result.IsAudio)
                {
                    Log.Information($"Opening audio player for: {node.Name}");
                    OpenAudioPlayer(node, result);
                }
                else if (result.MaterialData != null || result.AssetCategory.IsOfCategory(EAssetCategory.Materials))
                {

                    node.IsMaterial = true;

                    Log.Information($"Opening material viewer for: {node.Name}");
                    OpenMaterialViewer(node, result);
                }
                else if (result.IsModel || result.AssetCategory == EAssetCategory.StaticMesh || result.AssetCategory == EAssetCategory.SkeletalMesh)
                {

                    if (result.AssetCategory == EAssetCategory.SkeletalMesh)
                    {
                        node.IsSkeletalMesh = true;
                    }

                    Log.Information($"Opening model viewer for: {node.Name}");
                    _isViewerOpen = true;
                    ModelViewerService.LaunchPakViewer(provider, node.FullPath, result.ModelName ?? node.Name);
                    _isViewerOpen = false;

                    _previewService.ClearCacheFor(node.FullPath);
                    ViewModel.GetPakLoader().ClearPackageCache();
                }
                else if (result.AssetCategory.IsOfCategory(EAssetCategory.Animation) || result.AssetCategory == EAssetCategory.Skeleton)
                {

                    if (result.AssetCategory.IsOfCategory(EAssetCategory.Animation))
                    {
                        node.IsAnimation = true;
                    }

                    Log.Information($"Opening animation viewer for: {node.Name}");
                    _isViewerOpen = true;
                    ModelViewerService.LaunchPakViewer(provider, node.FullPath, node.Name);
                    _isViewerOpen = false;

                    _previewService.ClearCacheFor(node.FullPath);
                    ViewModel.GetPakLoader().ClearPackageCache();
                }
                else
                {
                    Log.Information($"No viewer available for file type: {node.Extension}");
                }
            }
            catch (Exception ex)
            {
                Log.Error(ex, $"Failed to open viewer for {node.Name}");
            }
        }
    }

    private void FileItem_RightTapped(object sender, RightTappedRoutedEventArgs e)
    {

    }

    private void CopyPath_Click(object sender, RoutedEventArgs e)
    {
        if (sender is MenuFlyoutItem item && item.Tag is FileNodeViewModel node)
        {
            var dataPackage = new Windows.ApplicationModel.DataTransfer.DataPackage();
            dataPackage.SetText(node.FullPath);
            Windows.ApplicationModel.DataTransfer.Clipboard.SetContent(dataPackage);
            Log.Information($"Copied path to clipboard: {node.FullPath}");
        }
    }

    private async void OpenPath_Click(object sender, RoutedEventArgs e)
    {
        if (sender is MenuFlyoutItem item && item.Tag is FileNodeViewModel node)
        {
            try
            {
                Log.Information($"OpenPath clicked for: {node.FullPath}");

                SwitchToTab(1);
                Log.Information("Switched to Folders tab");

                ViewModel.FolderSearchText = "";
                Log.Information("Cleared search");

                await Task.Delay(200);

                var foundNode = FindNodeInTreeWithPath(ViewModel.FileTree, node.FullPath, new List<FileNodeViewModel>());
                if (foundNode == null)
                {
                    Log.Error($"Could not find node in tree: {node.FullPath}");
                    return;
                }

                var (targetNode, pathParts) = foundNode.Value;
                Log.Information($"Path has {pathParts.Count} parts: {string.Join(" > ", pathParts.Select(n => n.Name))}");

                foreach (var part in pathParts)
                {
                    if (part.IsDirectory)
                    {
                        part.IsExpanded = true;
                        Log.Information($"Expanded: {part.Name}");
                    }
                }

                await Task.Delay(400);

                FoldersTreeView.UpdateLayout();

                if (!targetNode.IsDirectory && targetNode.Parent != null)
                {
                    ViewModel.SelectedNode = targetNode.Parent;
                    ViewModel.SelectedPreviewNode = targetNode.Parent;
                    Log.Information($"Selected parent folder: {targetNode.Parent.Name}");
                    await Task.Delay(300);

                    ViewModel.SelectedPreviewNode = targetNode;
                    Log.Information($"Set ViewModel.SelectedPreviewNode to: {targetNode.Name}");

                    await Task.Delay(200);
                    ExploreGridView.UpdateLayout();

                    ExploreGridView.SelectedItem = targetNode;
                    ExploreGridView.ScrollIntoView(targetNode, ScrollIntoViewAlignment.Default);
                    Log.Information($"Selected and scrolled GridView to: {targetNode.Name}");
                }
                else
                {

                    ViewModel.SelectedNode = targetNode;
                    ViewModel.SelectedPreviewNode = targetNode;
                    Log.Information($"Set ViewModel.SelectedNode to folder: {targetNode.Name}");
                }

                await Task.Delay(100);

                var scrolled = await ScrollTreeToNodeAsync(targetNode);
                if (scrolled)
                {
                    Log.Information($"Successfully scrolled to node in tree");
                }

                Log.Information($"Navigation complete for: {node.FullPath}");
            }
            catch (Exception ex)
            {
                Log.Error(ex, $"Failed to navigate to path: {ex.Message}");
            }
        }
        else
        {
            Log.Warning("OpenPath_Click: sender or Tag is null");
        }
    }

    private async void TextPreview_AssetPathClicked(string assetPath)
    {
        try
        {
            await NavigateToAssetPathAsync(assetPath);
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Failed to navigate from JSON asset path: {assetPath}");
        }
    }

    private async Task NavigateToAssetPathAsync(string assetPath)
    {
        if (string.IsNullOrWhiteSpace(assetPath) || ViewModel.FileTree.Count == 0)
            return;

        var targetNode = FindNodeByAssetPath(assetPath);
        if (targetNode == null)
        {
            Log.Information($"No matching node found for JSON path: {assetPath}");
            return;
        }

        await NavigateToNodeAsync(targetNode, assetPath);
    }

    private async Task NavigateToNodeAsync(FileNodeViewModel targetNode, string sourcePath)
    {
        SwitchToTab(1);
        ViewModel.FolderSearchText = "";

        var foundNode = FindNodeInTreeWithPath(ViewModel.FileTree, targetNode.FullPath, new List<FileNodeViewModel>());
        if (foundNode == null)
        {
            Log.Warning($"Could not resolve navigation node for path: {sourcePath}");
            return;
        }

        var (resolvedNode, pathParts) = foundNode.Value;

        foreach (var part in pathParts)
        {
            if (part.IsDirectory)
                part.IsExpanded = true;
        }

        await Task.Delay(120);
        FoldersTreeView.UpdateLayout();

        if (!resolvedNode.IsDirectory && resolvedNode.Parent != null)
        {
            ViewModel.SelectedNode = resolvedNode.Parent;
            ViewModel.SelectedPreviewNode = resolvedNode.Parent;
            await Task.Delay(80);
            ViewModel.SelectedPreviewNode = resolvedNode;

            ExploreGridView.UpdateLayout();
            ExploreGridView.SelectedItem = resolvedNode;
            ExploreGridView.ScrollIntoView(resolvedNode, ScrollIntoViewAlignment.Default);
        }
        else
        {
            ViewModel.SelectedNode = resolvedNode;
            ViewModel.SelectedPreviewNode = resolvedNode;
        }

        await Task.Delay(40);
        await ScrollTreeToNodeAsync(resolvedNode);
        Log.Information($"Navigated to asset path from JSON: {sourcePath} -> {resolvedNode.FullPath}");
    }

    private FileNodeViewModel? FindNodeByAssetPath(string assetPath)
    {
        var normalized = NormalizeAssetPath(assetPath);
        if (string.IsNullOrEmpty(normalized))
            return null;

        var candidates = BuildAssetPathCandidates(normalized);
        FileNodeViewModel? suffixMatch = null;
        int suffixScore = int.MinValue;

        foreach (var node in EnumerateNodes(ViewModel.FileTree))
        {
            if (node.IsDirectory)
                continue;

            var nodePath = NormalizeAssetPath(node.FullPath);
            if (string.IsNullOrEmpty(nodePath))
                continue;

            foreach (var candidate in candidates)
            {
                if (nodePath.Equals(candidate, StringComparison.OrdinalIgnoreCase))
                    return node;

                if (nodePath.EndsWith('/' + candidate, StringComparison.OrdinalIgnoreCase))
                {
                    var score = candidate.Length;
                    if (score > suffixScore)
                    {
                        suffixScore = score;
                        suffixMatch = node;
                    }
                }
            }
        }

        return suffixMatch;
    }

    private static List<string> BuildAssetPathCandidates(string normalized)
    {
        var values = new List<string>();
        AddCandidate(values, normalized);

        var withoutObjectSuffix = RemoveObjectSuffix(normalized);
        AddCandidate(values, withoutObjectSuffix);
        AddCandidate(values, withoutObjectSuffix + ".uasset");
        AddCandidate(values, withoutObjectSuffix + ".uexp");

        if (normalized.StartsWith("Game/", StringComparison.OrdinalIgnoreCase))
        {
            var gameRelative = normalized.Substring("Game/".Length);
            AddCandidate(values, gameRelative);
            AddCandidate(values, "Content/" + gameRelative);
            AddCandidate(values, RemoveObjectSuffix("Content/" + gameRelative) + ".uasset");
        }

        var contentIndex = normalized.IndexOf("/Content/", StringComparison.OrdinalIgnoreCase);
        if (contentIndex >= 0 && contentIndex + 1 < normalized.Length)
        {
            AddCandidate(values, normalized.Substring(contentIndex + 1));
        }

        return values;
    }

    private static void AddCandidate(List<string> values, string? value)
    {
        if (string.IsNullOrWhiteSpace(value))
            return;

        var normalized = NormalizeAssetPath(value);
        if (string.IsNullOrWhiteSpace(normalized))
            return;

        if (!values.Any(v => v.Equals(normalized, StringComparison.OrdinalIgnoreCase)))
            values.Add(normalized);
    }

    private static string RemoveObjectSuffix(string value)
    {
        var lastSlash = value.LastIndexOf('/');
        var lastDot = value.LastIndexOf('.');
        if (lastDot > lastSlash)
            return value.Substring(0, lastDot);

        return value;
    }

    private static string NormalizeAssetPath(string path)
    {
        if (string.IsNullOrWhiteSpace(path))
            return string.Empty;

        return path.Trim().Trim('"').Replace('\\', '/').TrimStart('/');
    }

    private static IEnumerable<FileNodeViewModel> EnumerateNodes(IEnumerable<FileNodeViewModel> nodes)
    {
        foreach (var node in nodes)
        {
            yield return node;

            if (node.Children.Count > 0)
            {
                foreach (var child in EnumerateNodes(node.Children))
                    yield return child;
            }
        }
    }

    private (FileNodeViewModel node, List<FileNodeViewModel> path)? FindNodeInTreeWithPath(
        IEnumerable<FileNodeViewModel> nodes,
        string fullPath,
        List<FileNodeViewModel> currentPath)
    {
        foreach (var node in nodes)
        {
            var newPath = new List<FileNodeViewModel>(currentPath) { node };

            if (node.FullPath == fullPath)
            {
                return (node, newPath);
            }

            if (node.Children.Count > 0)
            {
                var found = FindNodeInTreeWithPath(node.Children, fullPath, newPath);
                if (found != null)
                {
                    return found;
                }
            }
        }
        return null;
    }

    private void CopyItemName_Click(object sender, RoutedEventArgs e)
    {
        if (sender is MenuFlyoutItem item && item.Tag is FileNodeViewModel node)
        {
            var dataPackage = new Windows.ApplicationModel.DataTransfer.DataPackage();
            dataPackage.SetText(node.Name);
            Windows.ApplicationModel.DataTransfer.Clipboard.SetContent(dataPackage);
            Log.Information($"Copied item name to clipboard: {node.Name}");
        }
    }

    private async Task<bool> ScrollTreeToNodeAsync(FileNodeViewModel node)
    {
        await Task.Yield();
        FoldersTreeView.UpdateLayout();
        var container = FindTreeViewItem(FoldersTreeView, node) ?? await RetryFindTreeViewItemAsync(node);
        if (container != null)
        {
            container.IsSelected = true;
            container.StartBringIntoView(new BringIntoViewOptions { AnimationDesired = true });
            return true;
        }
        return false;
    }

    private async Task<TreeViewItem?> RetryFindTreeViewItemAsync(FileNodeViewModel node)
    {
        await Task.Delay(50);
        FoldersTreeView.UpdateLayout();
        return FindTreeViewItem(FoldersTreeView, node);
    }

    private TreeViewItem? FindTreeViewItem(DependencyObject root, FileNodeViewModel node)
    {
        if (root is TreeViewItem item && item.DataContext == node)
        {
            return item;
        }

        var childCount = VisualTreeHelper.GetChildrenCount(root);
        for (var i = 0; i < childCount; i++)
        {
            var child = VisualTreeHelper.GetChild(root, i);
            var result = FindTreeViewItem(child, node);
            if (result != null)
            {
                return result;
            }
        }

        return null;
    }

    public Visibility ModelViewerVisibility { get; private set; } = Visibility.Collapsed;
    private string? _currentModelName;

    public Visibility FileInfoVisibility => ViewModel.SelectedPreviewNode != null ? Visibility.Visible : Visibility.Collapsed;
    public Visibility EmptyStateVisibility => ViewModel.SelectedPreviewNode == null ? Visibility.Visible : Visibility.Collapsed;

    private void View3D_Click(object sender, RoutedEventArgs e)
    {
        if (ViewModel.SelectedPreviewNode != null && _currentModelName != null)
        {
            var provider = ViewModel.GetFileProvider();
            if (provider != null)
            {
                _isViewerOpen = true;
                ModelViewerService.LaunchPakViewer(provider, ViewModel.SelectedPreviewNode.FullPath, _currentModelName);
                _isViewerOpen = false;

                _previewService.ClearCacheFor(ViewModel.SelectedPreviewNode.FullPath);
                ViewModel.GetPakLoader().ClearPackageCache();
            }
        }
    }

    private void ExploreGrid_ItemClick(object sender, ItemClickEventArgs e)
    {
        if (e.ClickedItem is FileNodeViewModel node)
        {
            if (node.IsDirectory)
            {
                ViewModel.SelectedNode = node;
            }
            else
            {

                if (ViewModel.Packages.Contains(node))
                {
                    ViewModel.SelectedPackage = node;
                }
                else
                {
                    ViewModel.SelectedPreviewNode = node;
                }
            }
        }
    }

    private async void ExploreGrid_DoubleTapped(object sender, DoubleTappedRoutedEventArgs e)
    {
        if (e.OriginalSource is FrameworkElement element && element.DataContext is FileNodeViewModel node)
        {
            Log.Information($"Double-clicked on: {node.Name} (Extension: {node.Extension})");

            if (!node.IsDirectory)
            {
                try
                {
                    var provider = ViewModel.GetFileProvider();
                    if (provider != null)
                    {
                        Log.Information($"Loading preview for: {node.FullPath}");
                        var result = await _previewService.LoadFileAsync(provider, node.FullPath);

                        Log.Information($"Preview loaded - IsAudio: {result.IsAudio}, IsTexture: {result.IsTexture}, HasMaterialData: {result.MaterialData != null}, IsModel: {result.IsModel}");

                        ApplyAssetCategoryToNodeIcon(node, result.AssetCategory);

                        if (result.IsAudio)
                        {
                            OpenAudioPlayer(node, result);
                            return;
                        }

                        if (result.IsTexture)
                        {
                            Log.Information("Opening image viewer for texture");
                            await OpenImageViewerAsync(node);
                            return;
                        }

                        if (result.MaterialData != null)
                        {
                            Log.Information($"Opening material viewer with {result.MaterialData.TextureParams.Count} textures");
                            OpenMaterialViewer(node, result);
                            return;
                        }

                        if (result.IsModel || result.AssetCategory == EAssetCategory.Animation || result.AssetCategory == EAssetCategory.Skeleton)
                        {
                            _isViewerOpen = true;
                            ModelViewerService.LaunchPakViewer(provider, node.FullPath, result.ModelName ?? node.Name);
                            _isViewerOpen = false;

                            _previewService.ClearCacheFor(node.FullPath);
                            ViewModel.GetPakLoader().ClearPackageCache();
                            return;
                        }
                    }
                    else
                    {
                        Log.Warning("Provider is null");
                    }
                }
                catch (Exception ex)
                {
                    Log.Error(ex, $"Failed to open asset: {node.Name}");
                }
            }
        }
    }

    private void ApplyAssetCategoryToNodeIcon(FileNodeViewModel node, EAssetCategory category)
    {
        switch (category)
        {
            case EAssetCategory.SkeletalMesh:
            case EAssetCategory.Skeleton:
                node.IsSkeletalMesh = true;
                break;
            case EAssetCategory.StaticMesh:
            case EAssetCategory.NaniteDisplacedMesh:

                node.IsSkeletalMesh = true;
                break;
            case EAssetCategory.Animation:
                node.IsAnimation = true;
                break;
            case EAssetCategory.Texture:
                node.IsTexture = true;
                break;
            case EAssetCategory.Material:
            case EAssetCategory.MaterialEditorData:
            case EAssetCategory.MaterialFunction:
            case EAssetCategory.MaterialFunctionEditorData:
            case EAssetCategory.MaterialParameterCollection:
                node.IsMaterial = true;
                break;
        }
    }

    private async void PackagesList_DoubleTapped(object sender, DoubleTappedRoutedEventArgs e)
    {
        if (e.OriginalSource is FrameworkElement element && element.DataContext is FileNodeViewModel node)
        {
            Log.Information($"Double-clicked on: {node.Name} (Extension: {node.Extension})");

            if (!node.IsDirectory)
            {
                try
                {
                    var provider = ViewModel.GetFileProvider();
                    if (provider != null)
                    {
                        Log.Information($"Loading preview for: {node.FullPath}");
                        var result = await _previewService.LoadFileAsync(provider, node.FullPath);

                        Log.Information($"Preview loaded - IsAudio: {result.IsAudio}, IsTexture: {result.IsTexture}, HasMaterialData: {result.MaterialData != null}, IsModel: {result.IsModel}");

                        ApplyAssetCategoryToNodeIcon(node, result.AssetCategory);

                        if (result.IsAudio)
                        {
                            OpenAudioPlayer(node, result);
                            return;
                        }

                        if (result.IsTexture)
                        {
                            Log.Information("Opening image viewer for texture");
                            await OpenImageViewerAsync(node);
                            return;
                        }

                        if (result.MaterialData != null)
                        {
                            Log.Information($"Opening material viewer with {result.MaterialData.TextureParams.Count} textures");
                            OpenMaterialViewer(node, result);
                            return;
                        }

                        if (result.IsModel || result.AssetCategory == EAssetCategory.Animation || result.AssetCategory == EAssetCategory.Skeleton)
                        {
                            _isViewerOpen = true;
                            ModelViewerService.LaunchPakViewer(provider, node.FullPath, result.ModelName ?? node.Name);
                            _isViewerOpen = false;

                            _previewService.ClearCacheFor(node.FullPath);
                            ViewModel.GetPakLoader().ClearPackageCache();
                            return;
                        }
                    }
                    else
                    {
                        Log.Warning("Provider is null");
                    }
                }
                catch (Exception ex)
                {
                    Log.Error(ex, $"Failed to open asset: {node.Name}");
                }
            }
        }
    }

    private async Task OpenModelViewerAsync(FileNodeViewModel node)
    {
        ViewModel.SelectedPreviewNode = node;
        await LoadFilePreview(node);
        if (_currentModelName == null)
        {
            return;
        }

        var provider = ViewModel.GetFileProvider();
        if (provider == null)
        {
            return;
        }

        var parentWindow = WinRT.Interop.WindowNative.GetWindowHandle(this);
        _isViewerOpen = true;
        ModelViewerService.LaunchPakViewer(provider, node.FullPath, _currentModelName, parentWindow);
        _isViewerOpen = false;
    }

    private async Task OpenImageViewerAsync(FileNodeViewModel node)
    {
        try
        {
            var provider = ViewModel.GetFileProvider();
            if (provider == null) return;

            var dispatcher = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();
            if (dispatcher == null) return;

            var image = await LoadViewerImageAsync(provider, node.FullPath, dispatcher);
            var imageSource = image ?? node.Thumbnail;
            string? statusText = null;
            if (image == null)
            {
                statusText = node.Thumbnail != null
                    ? "Preview not available. Showing thumbnail."
                    : "Preview not available for this texture.";
            }

            var window = new ImageViewerWindow();
            window.SetImage(node.Name, node.FullPath, imageSource, statusText);
            _isViewerOpen = true;
            window.Closed += (_, _) =>
            {
                _imageWindows.Remove(window);
                _isViewerOpen = _imageWindows.Count > 0;

                _previewService.ClearCacheFor(node.FullPath);

                GC.Collect(GC.MaxGeneration, GCCollectionMode.Optimized);
                GC.WaitForPendingFinalizers();
            };
            _imageWindows.Add(window);

            while (_imageWindows.Count > MaxOpenViewerWindows)
            {
                var oldest = _imageWindows[0];
                _imageWindows.RemoveAt(0);
                oldest.Close();
            }
            window.Activate();
            window.AppWindow?.MoveInZOrderAtTop();
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Failed to open image viewer for {node.Name}");
        }
    }

    private void OpenMaterialViewer(FileNodeViewModel node, PreviewResult result)
    {
        try
        {

            var provider = ViewModel.GetFileProvider();
            if (provider != null)
            {
                Log.Information($"Launching 3D cube viewer for material: {node.Name}");
                var parentWindow = WinRT.Interop.WindowNative.GetWindowHandle(this);
                _isViewerOpen = true;
                ModelViewerService.LaunchPakViewerWithMaterial(provider, node.FullPath, node.Name, result.MaterialData, parentWindow);
                _isViewerOpen = false;

                _previewService.ClearCacheFor(node.FullPath);
                ViewModel.GetPakLoader().ClearPackageCache();
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Failed to open material viewer for {node.Name}");
        }
    }

    private void OpenAudioPlayer(FileNodeViewModel node, PreviewResult result)
    {
        try
        {
            var window = new AudioPlayerWindow();

            if (result.AudioSounds != null && result.AudioSounds.Count > 0)
            {
                window.SetSoundBank(node.Name, node.FullPath, result.AudioSounds);
            }
            else if (result.AudioData != null)
            {
                window.SetAudio(node.Name, node.FullPath, result.AudioData, result.AudioFormat ?? "wav");
            }

            window.Closed += (_, _) =>
            {
                _isViewerOpen = false;
                _previewService.ClearCacheFor(node.FullPath);
                GC.Collect(GC.MaxGeneration, GCCollectionMode.Optimized);
                GC.WaitForPendingFinalizers();
            };
            _isViewerOpen = true;
            window.Activate();
            window.AppWindow?.MoveInZOrderAtTop();
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Failed to open audio player for {node.Name}");
        }
    }

    private static async Task<Microsoft.UI.Xaml.Media.Imaging.BitmapImage?> LoadViewerImageAsync(
        CUE4Parse.FileProvider.IFileProvider provider,
        string filePath,
        Microsoft.UI.Dispatching.DispatcherQueue dispatcher)
    {
        var ext = Path.GetExtension(filePath).ToLowerInvariant();
        var isStandaloneImage = ext == ".png" || ext == ".jpg" || ext == ".bmp" || ext == ".svg";

        if (!isStandaloneImage && OodleHelper.Instance == null)
        {
            return null;
        }

        return await TextureLoader.LoadFullImageAsync(provider, filePath, dispatcher);
    }

    #region Conflict Dashboard

    private bool _isConflictMode = false;
    private Models.ConflictDashboardData? _conflictDashboardData;
    private List<FileNodeViewModel> _currentModFileTree = new();

    public void NavDashboardButton_Click(object sender, RoutedEventArgs e)
    {
        ShowDashboardView();
    }

    public void NavExplorerButton_Click(object sender, RoutedEventArgs e)
    {
        ShowFullView();
    }

    public void ShowDashboardView()
    {
        FullViewGrid.Visibility = Visibility.Collapsed;
        if (MinimalViewGrid != null) MinimalViewGrid.Visibility = Visibility.Collapsed;
        DashboardContainer.Visibility = Visibility.Visible;

        if (GameSelectorPanel != null) GameSelectorPanel.Visibility = Visibility.Collapsed;
        if (LoadGameButton != null) LoadGameButton.Visibility = Visibility.Collapsed;
    }

    public void ShowFullView()
    {
        DashboardContainer.Visibility = Visibility.Collapsed;
        if (MinimalViewGrid != null) MinimalViewGrid.Visibility = Visibility.Collapsed;
        FullViewGrid.Visibility = Visibility.Visible;

        if (GameSelectorPanel != null) GameSelectorPanel.Visibility = Visibility.Visible;
        if (LoadGameButton != null) LoadGameButton.Visibility = Visibility.Visible;
    }

    private void PopulateDashboard(string modFilePath, List<FileNodeViewModel> fileTree)
    {
        _currentModFileTree = fileTree;
        DashboardControl.Populate(modFilePath, fileTree, _conflictDashboardData);
    }

    #endregion
}
