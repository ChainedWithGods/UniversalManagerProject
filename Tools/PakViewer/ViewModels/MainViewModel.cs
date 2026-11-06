using System.Collections.ObjectModel;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Windows.Input;
using System.Threading;
using CUE4Parse.FileProvider;
using CUE4Parse.UE4.Assets;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Data;
using PakViewer.Models;
using PakViewer.Services;
using PakViewer.Settings;
using PakViewer.Helpers;

namespace PakViewer.ViewModels;

public enum ArchiveSortOption
{
         NameAsc,
         NameDesc,
         FileCountAsc,
         FileCountDesc,
         SizeAsc,
         SizeDesc
}

public enum ArchiveFilterOption
{
         All,
         ModsOnly,
         GameFilesOnly
}

public enum PackageSortOption
{
         NameAsc,
         NameDesc,
         SizeAsc,
         SizeDesc,
         ExtensionAsc,
         ExtensionDesc
}

public class MainViewModel : ViewModelBase
{
         public event EventHandler? SettingsRequested;
         public event EventHandler? GameLoading;

         private readonly PakLoaderService _pakLoader;
         private readonly Microsoft.UI.Dispatching.DispatcherQueue _dispatcherQueue;
         private GameConfig? _selectedGame;
         private ArchiveViewModel? _selectedArchive;
         private FileNodeViewModel? _selectedNode;
         private FileNodeViewModel? _selectedPackage;
         private FileNodeViewModel? _selectedPreviewNode;
         private string _statusText = "Ready";
         private bool _isLoading;

         private CancellationTokenSource? _searchCts;
         private readonly object _searchLock = new object();

         private CancellationTokenSource? _thumbnailCts;
         private CancellationTokenSource? _packageResolutionCts;
        private readonly Stack<FileNodeViewModel> _backStack = new();
        private readonly Stack<FileNodeViewModel> _forwardStack = new();
        private bool _isHistoryNavigation;

         public MainViewModel()
         {
                  _dispatcherQueue = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();
                  _pakLoader = new PakLoaderService();

                  _pakLoader.StatusChanged += message =>
                  {
                           _dispatcherQueue.TryEnqueue(() =>
                           {
                                    if (IsLoading)
                                    {
                                             StatusText = message;
                                    }
                           });
                  };
                  Games = AppSettings.Instance.Games;
                  FileTree = new RangeObservableCollection<FileNodeViewModel>();

                  LoadGameCommand = new RelayCommand(async () => await LoadGame(), () => SelectedGame != null);
                  OpenSettingsCommand = new RelayCommand(OpenSettings);

                  ApplyArchivesViewSettings();
                  ApplyPackagesViewSettings();

                  if (!string.IsNullOrEmpty(AppSettings.Instance.LastSelectedGame))
                  {
                           SelectedGame = Games.FirstOrDefault(g => g.Name == AppSettings.Instance.LastSelectedGame);
                  }

                  if (AppSettings.Instance.AutoLoadLastProfile && SelectedGame != null)
                  {
                      _ = LoadGame();
                  }
         }

         public ObservableCollection<GameConfig> Games { get; }
         public ObservableCollection<ArchiveViewModel> Archives { get; } = new();
         public ObservableCollection<ArchiveViewModel> FilteredArchives { get; } = new();
         public RangeObservableCollection<FileNodeViewModel> FileTree { get; }
         public RangeObservableCollection<FileNodeViewModel> FilteredFileTree { get; } = new();
         public ObservableCollection<FileNodeViewModel> Packages { get; } = new();
         public ObservableCollection<FileNodeViewModel> FilteredPackages { get; } = new();

         private string _folderSearchText = string.Empty;
         public string FolderSearchText
         {
                  get => _folderSearchText;
                  set
                  {
                           if (SetProperty(ref _folderSearchText, value))
                           {
                                    _ = DebouncedSearchAsync(value);
                                    OnPropertyChanged(nameof(IsSearchActive));
                                    OnPropertyChanged(nameof(ShowTreeView));
                                    OnPropertyChanged(nameof(ShowSearchResults));
                           }
                  }
         }

         private async Task DebouncedSearchAsync(string query)
         {
             lock (_searchLock)
             {
                 _searchCts?.Cancel();
                 _searchCts = new CancellationTokenSource();
             }

             var token = _searchCts.Token;

             try
             {
                 await Task.Delay(300, token);

                 if (token.IsCancellationRequested) return;

                 var matchingFiles = new List<FileNodeViewModel>();

                 if (!string.IsNullOrWhiteSpace(query))
                 {
                     await Task.Run(() =>
                     {
                         CollectMatchingFiles(FileTree, query.Trim(), matchingFiles);
                     }, token);
                 }

                 if (token.IsCancellationRequested) return;

                 _dispatcherQueue.TryEnqueue(() =>
                 {
                     if (token.IsCancellationRequested) return;

                     if (string.IsNullOrWhiteSpace(query))
                     {
                         FilteredFileTree.ReplaceRange(FileTree);
                     }
                     else
                     {
                         FilteredFileTree.ReplaceRange(matchingFiles.OrderBy(f => f.Name));
                     }

                     OnPropertyChanged(nameof(HasLoadedFolders));
                     OnPropertyChanged(nameof(ShowTreeView));
                     OnPropertyChanged(nameof(ShowSearchResults));
                 });

                 await LoadSearchResultIconsAsync(matchingFiles, token);
             }
             catch (TaskCanceledException)
             {

             }
         }

        private async Task LoadSearchResultIconsAsync(List<FileNodeViewModel> matchingFiles, CancellationToken token)
        {
            if (matchingFiles.Count == 0) return;

            var provider = _pakLoader.GetProvider();
            if (provider == null) return;

            var dispatcher = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();
            if (dispatcher == null) return;

            var fileItems = matchingFiles.Where(i => !i.IsDirectory && i.Extension == "uasset").ToList();
            if (fileItems.Count == 0) return;

            await Task.Run(async () =>
            {
                var batchSize = 6;

                for (int i = 0; i < fileItems.Count; i += batchSize)
                {
                    if (token.IsCancellationRequested) break;

                    var batch = fileItems.Skip(i).Take(batchSize);
                    var tasks = batch.Select(item =>
                    {
                        if (token.IsCancellationRequested) return Task.CompletedTask;

                        var category = TryResolveAssetCategory(provider, item.FullPath);
                        if (category.HasValue && !token.IsCancellationRequested)
                        {
                            dispatcher.TryEnqueue(() =>
                            {
                                if (!token.IsCancellationRequested)
                                {
                                    ApplyResolvedAssetCategory(item, category.Value);
                                }
                            });
                        }

                        return Task.CompletedTask;
                    });

                    await Task.WhenAll(tasks);
                    await Task.Delay(5, token);
                }
            }, token);
        }

         public bool IsSearchActive => !string.IsNullOrWhiteSpace(_folderSearchText);

         public bool HasLoadedFolders => FileTree.Count > 0;

         public bool ShowTreeView => HasLoadedFolders && !IsSearchActive;

         public bool ShowSearchResults => HasLoadedFolders && IsSearchActive;

         private FileNodeViewModel? _selectedSearchResult;
         public FileNodeViewModel? SelectedSearchResult
         {
                  get => _selectedSearchResult;
                  set
                  {
                           if (SetProperty(ref _selectedSearchResult, value))
                           {
                                    if (value != null)
                                    {
                                             SelectedPreviewNode = value;
                                    }
                           }
                  }
         }

         private bool _isExploreMode;
         public bool IsExploreMode
         {
             get => _isExploreMode;
             set
             {
                 if (SetProperty(ref _isExploreMode, value))
                 {
                     OnPropertyChanged(nameof(ExploreGridVisibility));
                     OnPropertyChanged(nameof(PreviewTextVisibility));
                 }
             }
         }

         public Visibility ExploreGridVisibility => IsExploreMode ? Visibility.Visible : Visibility.Collapsed;
         public Visibility PreviewTextVisibility => IsExploreMode ? Visibility.Collapsed : Visibility.Visible;

         public ObservableCollection<FileNodeViewModel> ExploreItems { get; } = new();
        public bool CanGoBack => _backStack.Count > 0;
        public bool CanGoForward => _forwardStack.Count > 0;

         public async void UpdateExploreItems(FileNodeViewModel? node)
         {
             _thumbnailCts?.Cancel();
             _thumbnailCts = new CancellationTokenSource();
             var token = _thumbnailCts.Token;

             ExploreItems.Clear();
             if (node == null) return;

             IEnumerable<FileNodeViewModel>? items = null;

             if (node.IsDirectory)
             {
                 items = node.Children;
             }
             else if (node.Parent != null)
             {
                 items = node.Parent.Children;
             }

             if (items != null)
             {
                 foreach (var item in items.OrderByDescending(i => i.IsDirectory).ThenBy(i => i.Name))
                 {
                     ExploreItems.Add(item);
                 }

                 var provider = _pakLoader.GetProvider();
                 if (provider != null)
                 {
                     var dispatcher = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();
                     if (dispatcher == null)
                     {
                         return;
                     }

                     var fileItems = ExploreItems.Where(i => !i.IsDirectory).ToList();

                     _ = Task.Run(async () =>
                     {
                         var batchSize = 4;

                         for (int i = 0; i < fileItems.Count; i += batchSize)
                         {
                             if (token.IsCancellationRequested) break;

                             var batch = fileItems.Skip(i).Take(batchSize);
                            var tasks = batch.Select(async item =>
                            {
                                if (token.IsCancellationRequested) return;

                                EAssetCategory? resolvedCategory = null;
                                if (item.Extension == "uasset")
                                {
                                    if (!item.IsResolved)
                                    {
                                        resolvedCategory = TryResolveAssetCategory(provider, item.FullPath);
                                        if (resolvedCategory.HasValue && !token.IsCancellationRequested)
                                        {
                                            dispatcher.TryEnqueue(() =>
                                            {
                                                if (!token.IsCancellationRequested)
                                                {
                                                    ApplyResolvedAssetCategory(item, resolvedCategory.Value);
                                                }
                                            });
                                        }
                                    }

                                    var shouldLoadThumbnail = item.IsTexture || resolvedCategory == EAssetCategory.Texture;
                                    if (item.Thumbnail != null || !shouldLoadThumbnail) return;
                                }
                                else
                                {
                                    return;
                                }

                                var thumb = await TextureLoader.LoadThumbnailAsync(provider, item.FullPath, dispatcher);
                                if (thumb != null && !token.IsCancellationRequested)
                                {
                                    dispatcher.TryEnqueue(() =>
                                    {
                                        if (!token.IsCancellationRequested)
                                        {
                                            item.Thumbnail = thumb;
                                        }
                                    });
                                }
                            });

                             await Task.WhenAll(tasks);

                             await Task.Delay(10, token);
                         }
                     }, token);
                 }
             }
         }

         private static EAssetCategory? TryResolveAssetCategory(DefaultFileProvider provider, string filePath)
         {
             try
             {
                 if (provider.TryLoadPackage(filePath, out var package))
                 {
                     return AssetTypeDetector.Detect(package, Path.GetFileNameWithoutExtension(filePath)).Category;
                 }
             }
             catch
             {
             }

             return null;
         }

         private static void ApplyResolvedAssetCategory(FileNodeViewModel item, EAssetCategory category)
         {
             item.IsResolved = true;
             item.IsTexture = category == EAssetCategory.Texture;
             item.IsMaterial = category.IsOfCategory(EAssetCategory.Materials);
             item.IsSkeletalMesh = category == EAssetCategory.SkeletalMesh || category == EAssetCategory.StaticMesh || category == EAssetCategory.NaniteDisplacedMesh || category == EAssetCategory.Skeleton;
             item.IsAnimation = category == EAssetCategory.Animation;
             item.IsSkeleton = category == EAssetCategory.Skeleton;
             item.IsPhysicsAsset = category == EAssetCategory.PhysicsAsset;
             item.IsAudioFile = category == EAssetCategory.Audio || category == EAssetCategory.SoundBank;
         }

        public void NavigateBack()
        {
            if (_backStack.Count == 0) return;

            var target = _backStack.Pop();
            if (_selectedPreviewNode != null)
            {
                _forwardStack.Push(_selectedPreviewNode);
            }

            NavigateToHistoryTarget(target);
            OnPropertyChanged(nameof(CanGoBack));
            OnPropertyChanged(nameof(CanGoForward));
        }

        public void NavigateForward()
        {
            if (_forwardStack.Count == 0) return;

            var target = _forwardStack.Pop();
            if (_selectedPreviewNode != null)
            {
                _backStack.Push(_selectedPreviewNode);
            }

            NavigateToHistoryTarget(target);
            OnPropertyChanged(nameof(CanGoBack));
            OnPropertyChanged(nameof(CanGoForward));
        }

        private void NavigateToHistoryTarget(FileNodeViewModel target)
        {
            _isHistoryNavigation = true;
            if (target.IsDirectory)
            {
                SelectedNode = target;
            }
            else
            {
                NavigateToFileParent(target);
                SelectedPreviewNode = target;
            }
            _isHistoryNavigation = false;
        }

        private void ClearNavigationHistory()
        {
            _backStack.Clear();
            _forwardStack.Clear();
            OnPropertyChanged(nameof(CanGoBack));
            OnPropertyChanged(nameof(CanGoForward));
        }

         public void RefreshFilteredFileTree()
         {
                  lock (_searchLock)
                  {
                      _searchCts?.Cancel();
                      _searchCts = null;
                  }

                  FilteredFileTree.Clear();
                  var searchQuery = _folderSearchText?.Trim() ?? string.Empty;

                  if (string.IsNullOrEmpty(searchQuery))
                  {

                           FilteredFileTree.ReplaceRange(FileTree);
                  }
                  else
                  {

                           var matchingFiles = new List<FileNodeViewModel>();
                           CollectMatchingFiles(FileTree, searchQuery, matchingFiles);

                           FilteredFileTree.ReplaceRange(matchingFiles.OrderBy(f => f.Name));
                  }

                  OnPropertyChanged(nameof(HasLoadedFolders));
                  OnPropertyChanged(nameof(ShowTreeView));
                  OnPropertyChanged(nameof(ShowSearchResults));
         }

         private void CollectMatchingFiles(IEnumerable<FileNodeViewModel> nodes, string searchQuery, List<FileNodeViewModel> results)
         {
                  foreach (var node in nodes)
                  {

                           bool matches = node.Name.Contains(searchQuery, StringComparison.OrdinalIgnoreCase) ||
                                          node.FullPath.Contains(searchQuery, StringComparison.OrdinalIgnoreCase);

                           if (matches)
                           {

                                    results.Add(node);
                           }

                           if (node.Children.Count > 0)
                           {
                                    CollectMatchingFiles(node.Children, searchQuery, results);
                           }
                  }
         }

         public FileNodeViewModel? NavigateToFileParent(FileNodeViewModel file)
         {
                  if (file == null || file.IsDirectory)
                           return null;

                  var fullPath = file.FullPath;
                  var lastSlash = fullPath.LastIndexOf('/');
                  if (lastSlash <= 0)
                  {

                           return null;
                  }

                  var parentPath = fullPath.Substring(0, lastSlash);

                  FileNodeViewModel? parentNode = null;
                  var pathParts = parentPath.Split('/', StringSplitOptions.RemoveEmptyEntries);
                  IEnumerable<FileNodeViewModel> currentNodes = FileTree;
                  string currentPath = "";

                  foreach (var part in pathParts)
                  {
                           currentPath = string.IsNullOrEmpty(currentPath) ? part : $"{currentPath}/{part}";

                           FileNodeViewModel? matchingNode = null;
                           foreach (var node in currentNodes)
                           {
                                    if (node.FullPath.Equals(currentPath, StringComparison.OrdinalIgnoreCase))
                                    {
                                             matchingNode = node;
                                             break;
                                    }
                           }

                           if (matchingNode == null)
                           {

                                    return null;
                           }

                           matchingNode.IsExpanded = true;
                           parentNode = matchingNode;
                           currentNodes = matchingNode.Children;
                  }

                  if (parentNode != null)
                  {

                           SelectedNode = parentNode;

                           foreach (var pkg in Packages)
                           {
                                    if (pkg.FullPath.Equals(file.FullPath, StringComparison.OrdinalIgnoreCase))
                                    {
                                             SelectedPackage = pkg;
                                             break;
                                    }
                           }
                  }

                  return parentNode;
         }

         private string _archivesSearchText = string.Empty;
         public string ArchivesSearchText
         {
                  get => _archivesSearchText;
                  set
                  {
                           if (SetProperty(ref _archivesSearchText, value))
                           {
                                    ApplyArchivesViewSettings();
                           }
                  }
         }

         private ArchiveSortOption _archivesSortOption = ArchiveSortOption.NameAsc;
         public ArchiveSortOption ArchivesSortOption
         {
                  get => _archivesSortOption;
                  set
                  {
                           if (SetProperty(ref _archivesSortOption, value))
                           {
                                    ApplyArchivesViewSettings();
                                    OnPropertyChanged(nameof(ArchivesSortIndex));
                           }
                  }
         }

         public int ArchivesSortIndex
         {
                  get => (int)_archivesSortOption;
                  set
                  {
                           ArchivesSortOption = (ArchiveSortOption)value;
                  }
         }

         private ArchiveFilterOption _archivesFilterOption = ArchiveFilterOption.All;
         public ArchiveFilterOption ArchivesFilterOption
         {
                  get => _archivesFilterOption;
                  set
                  {
                           if (SetProperty(ref _archivesFilterOption, value))
                           {
                                    ApplyArchivesViewSettings();
                                    OnPropertyChanged(nameof(ModCount));
                                    OnPropertyChanged(nameof(GameFileCount));
                                    OnPropertyChanged(nameof(ArchivesFilterIndex));
                           }
                  }
         }

         public int ArchivesFilterIndex
         {
                  get => (int)_archivesFilterOption;
                  set
                  {
                           ArchivesFilterOption = (ArchiveFilterOption)value;
                  }
         }

         public int ModCount => Archives.Count(a => a.IsMod);

         public int GameFileCount => Archives.Count(a => a.IsGameFile);

         private string _packagesSearchText = string.Empty;
         public string PackagesSearchText
         {
                  get => _packagesSearchText;
                  set
                  {
                           if (SetProperty(ref _packagesSearchText, value))
                           {
                                    ApplyPackagesViewSettings();
                           }
                  }
         }

         private PackageSortOption _packagesSortOption = PackageSortOption.NameAsc;
         public PackageSortOption PackagesSortOption
         {
                  get => _packagesSortOption;
                  set
                  {
                           if (SetProperty(ref _packagesSortOption, value))
                           {
                                    ApplyPackagesViewSettings();
                           }
                  }
         }

         public LoadingMode[] LoadingModes { get; } = (LoadingMode[])Enum.GetValues(typeof(LoadingMode));

         private LoadingMode _selectedLoadingMode = LoadingMode.Multiple;
         public LoadingMode SelectedLoadingMode
         {
                  get => _selectedLoadingMode;
                  set => SetProperty(ref _selectedLoadingMode, value);
         }

         private int _selectedTabIndex;
         public int SelectedTabIndex
         {
                  get => _selectedTabIndex;
                  set => SetProperty(ref _selectedTabIndex, value);
         }

         public GameConfig? SelectedGame
         {
                  get => _selectedGame;
                  set
                  {
                           if (SetProperty(ref _selectedGame, value))
                           {
                                    AppSettings.Instance.LastSelectedGame = value?.Name;
                                    AppSettings.Save();

                                    (LoadGameCommand as RelayCommand)?.RaiseCanExecuteChanged();
                           }
                  }
         }

         public ArchiveViewModel? SelectedArchive
         {
                  get => _selectedArchive;
                  set
                  {
                           if (SetProperty(ref _selectedArchive, value))
                           {
                                    if (value != null)
                                    {
                                            ClearNavigationHistory();
                                             LoadFoldersForArchive(value);
                                    }
                           }
                  }
         }

         public FileNodeViewModel? SelectedNode
         {
                  get => _selectedNode;
                  set
                  {

                           if (SetProperty(ref _selectedNode, value))
                           {
                                    UpdatePackages(value);
                                    if (value != null)
                                    {
                                        value.IsExpanded = true;

                                        var parent = value.Parent;
                                        while (parent != null)
                                        {
                                            parent.IsExpanded = true;
                                            parent = parent.Parent;
                                        }

                                        SelectedPreviewNode = value;
                                    }
                           }
                  }
         }

         public FileNodeViewModel? SelectedPackage
         {
                  get => _selectedPackage;
                  set
                  {
                           if (SetProperty(ref _selectedPackage, value))
                           {
                                    if (value != null) SelectedPreviewNode = value;
                           }
                  }
         }

         public FileNodeViewModel? SelectedPreviewNode
         {
                  get => _selectedPreviewNode;
                  set
                  {
                      var previous = _selectedPreviewNode;
                      if (SetProperty(ref _selectedPreviewNode, value))
                      {
                          if (!_isHistoryNavigation && previous != null && value != null && !ReferenceEquals(previous, value))
                          {
                              _backStack.Push(previous);
                              _forwardStack.Clear();
                              OnPropertyChanged(nameof(CanGoBack));
                              OnPropertyChanged(nameof(CanGoForward));
                          }

                          if (value != null && value.IsDirectory)
                          {
                              UpdateExploreItems(value);
                              IsExploreMode = true;
                          }
                      }
                  }
         }

         public string StatusText
         {
                  get => _statusText;
                  set => SetProperty(ref _statusText, value);
         }

         public bool IsLoading
         {
                  get => _isLoading;
                  set
                  {
                      if (SetProperty(ref _isLoading, value))
                      {
                          OnPropertyChanged(nameof(IsLoadingVisibility));
                      }
                  }
         }

         public Visibility IsLoadingVisibility => IsLoading ? Visibility.Visible : Visibility.Collapsed;

         private void UpdateStatus(string message)
         {
                  if (_dispatcherQueue.HasThreadAccess)
                  {
                           StatusText = message;
                  }
                  else
                  {
                           _dispatcherQueue.TryEnqueue(() => StatusText = message);
                  }
         }

         public ICommand LoadGameCommand { get; }
         public ICommand OpenSettingsCommand { get; }

         private async Task LoadGame()
         {
                  if (SelectedGame == null) return;

                  IsLoading = true;
                  StatusText = $"Loading {SelectedGame.Name}...";
                  FileTree.Clear();
                  Archives.Clear();
                  Packages.Clear();

                  GameLoading?.Invoke(this, EventArgs.Empty);

                  try
                  {

                           var dispatcherQueue = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();

                           await Task.Run(() =>
                           {

                                    _pakLoader.LoadPakFiles(SelectedGame);
                                    var archives = _pakLoader.GetMountedArchives();

                           var resolvedPakPath = _pakLoader.ResolvePakPath(SelectedGame.PakPath);
                           var modPaths = _pakLoader.DiscoverModsInSubdirectories(resolvedPakPath);

                                    dispatcherQueue?.TryEnqueue(() =>
                     {
                              Archives.Clear();

                              var vfsList = archives.Cast<object>().ToList();
                              var groups = vfsList.GroupBy(v =>
               {
                        var nameObj = v.GetType().GetProperty("Name")?.GetValue(v);
                        var name = nameObj?.ToString() ?? string.Empty;
                        var baseName = Path.GetFileNameWithoutExtension(name);
                        if (baseName.EndsWith("-x", StringComparison.OrdinalIgnoreCase))
                                 baseName = baseName[..^2];
                        return baseName;
               });

                              foreach (var g in groups)
                              {
                                       var members = g.ToList();
                                       object primary = members
                            .FirstOrDefault(m => string.Equals(Path.GetExtension(m.GetType().GetProperty("Name")?.GetValue(m)?.ToString() ?? string.Empty), ".utoc", StringComparison.OrdinalIgnoreCase))
                            ?? members[0];
                                       Archives.Add(new ArchiveViewModel(primary, members, isBaseGame: true));
                              }

                              foreach (var modPath in modPaths)
                              {
                                       var modName = Path.GetFileName(modPath);
                                       Archives.Add(new ArchiveViewModel(modPath, modName, isUnmounted: true));
                              }

                              ApplyArchivesViewSettings();
                     });
                           });

                           if (StatusText.StartsWith("Loading ", StringComparison.OrdinalIgnoreCase) ||
                               StatusText.Contains("Continuing", StringComparison.OrdinalIgnoreCase))
                           {
                                    StatusText = $"Loaded {SelectedGame.Name} - {_pakLoader.GetFileCount()} files (base game)";
                           }
                  }
                  catch (Exception ex)
                  {
                           StatusText = $"Error loading {SelectedGame.Name}";

                           Serilog.Log.Error(ex, "Failed to load pak files for {GameName}", SelectedGame.Name);
                  }
                  finally
                  {
                           IsLoading = false;
                  }
         }

         public void LoadFoldersForArchive(ArchiveViewModel archive)
         {
                  FileTree.Clear();
                  Packages.Clear();

                  GameLoading?.Invoke(this, EventArgs.Empty);

                  if (archive.IsUnmounted && !string.IsNullOrEmpty(archive.UnmountedPath))
                  {
                           StatusText = $"Mounting mod: {archive.Name}...";
                           var mounted = _pakLoader.RegisterAndMountMod(archive.UnmountedPath);
                           if (!mounted)
                           {
                                    StatusText = $"Failed to mount mod: {archive.Name}";
                                    return;
                           }
                           StatusText = $"Loaded mod: {archive.Name}";

                           var modTree = _pakLoader.GetModFileTree(archive.UnmountedPath);
                           FileTree.ReplaceRange(modTree);
                           RefreshFilteredFileTree();
                           return;
                  }

                  var roots = new List<FileNodeViewModel>();
                  var memberNames = archive.MemberNames;
                  if (archive.IsGroup && memberNames != null && memberNames.Count > 1)
                  {
                           var tree = _pakLoader.GetCombinedArchivesFileTree(memberNames);
                           roots.AddRange(tree);
                  }
                  else
                  {
                           var tree = _pakLoader.GetArchiveFileTree(archive.Name);
                           roots.AddRange(tree);
                  }
                  FileTree.ReplaceRange(roots);
                  RefreshFilteredFileTree();
         }

         public void LoadFoldersForArchives(System.Collections.IEnumerable archives)
         {
                  FileTree.Clear();
                  Packages.Clear();

                  var archiveNames = new List<string>();
                  var unmountedMods = new List<ArchiveViewModel>();
                  var roots = new List<FileNodeViewModel>();

                  foreach (var item in archives)
                  {
                           if (item is ArchiveViewModel a)
                           {

                                    if (a.IsUnmounted && !string.IsNullOrEmpty(a.UnmountedPath))
                                    {
                                             unmountedMods.Add(a);
                                    }
                                    else if (a.IsGroup && a.MemberNames != null && a.MemberNames.Count > 0)
                                    {
                                             archiveNames.AddRange(a.MemberNames);
                                    }
                                    else
                                    {
                                             archiveNames.Add(a.Name);
                                    }
                           }
                  }

                  foreach (var mod in unmountedMods)
                  {
                           StatusText = $"Mounting mod: {mod.Name}...";
                           var mounted = _pakLoader.RegisterAndMountMod(mod.UnmountedPath!);
                           if (mounted)
                           {

                                    var modTree = _pakLoader.GetModFileTree(mod.UnmountedPath!);
                                    foreach (var node in modTree)
                                    {

                                             node.SourceArchive = mod.Name;
                                    }
                                    roots.AddRange(modTree);
                           }
                  }

                  foreach (var archiveName in archiveNames)
                  {
                           var tree = _pakLoader.GetArchiveFileTree(archiveName);

                           var displayName = Path.GetFileNameWithoutExtension(archiveName);
                           foreach (var node in tree)
                           {
                                    node.SourceArchive = displayName;
                           }
                            roots.AddRange(tree);
                  }

                  if (unmountedMods.Count > 0 || archiveNames.Count > 0)
                  {
                           var totalCount = unmountedMods.Count + archiveNames.Count;
                           StatusText = $"Loaded {totalCount} archive(s)";
                  }

                  FileTree.ReplaceRange(roots);
                  RefreshFilteredFileTree();
         }

         public async Task LoadFoldersForArchivesAsync(IEnumerable<ArchiveViewModel> archives)
         {
                  var selectedArchives = archives?.ToList() ?? new List<ArchiveViewModel>();
                  if (selectedArchives.Count == 0)
                  {
                           StatusText = "No archives selected";
                           return;
                  }

                  IsLoading = true;
                  StatusText = $"Loading {selectedArchives.Count} archive(s)...";
                  FileTree.Clear();
                  Packages.Clear();

                  GameLoading?.Invoke(this, EventArgs.Empty);

                  await Task.Yield();

                  try
                  {
                           var result = await Task.Run(() =>
                           {
                                    var archiveNames = new List<string>();
                                    var unmountedMods = new List<ArchiveViewModel>();
                                    var roots = new List<FileNodeViewModel>();

                                    foreach (var archive in selectedArchives)
                                    {
                                             if (archive.IsUnmounted && !string.IsNullOrEmpty(archive.UnmountedPath))
                                             {
                                                      unmountedMods.Add(archive);
                                             }
                                             else if (archive.IsGroup && archive.MemberNames != null && archive.MemberNames.Count > 0)
                                             {
                                                      archiveNames.AddRange(archive.MemberNames);
                                             }
                                             else
                                             {
                                                      archiveNames.Add(archive.Name);
                                             }
                                    }

                                    foreach (var mod in unmountedMods)
                                    {
                                             UpdateStatus($"Mounting mod: {mod.Name}...");
                                             var mounted = _pakLoader.RegisterAndMountMod(mod.UnmountedPath!);
                                             if (!mounted)
                                             {
                                                      continue;
                                             }

                                             var modTree = _pakLoader.GetModFileTree(mod.UnmountedPath!);
                                             foreach (var node in modTree)
                                             {
                                                      node.SourceArchive = mod.Name;
                                             }
                                             roots.AddRange(modTree);
                                    }

                                    foreach (var archiveName in archiveNames)
                                    {
                                             UpdateStatus($"Loading archive: {Path.GetFileNameWithoutExtension(archiveName)}...");
                                             var tree = _pakLoader.GetArchiveFileTree(archiveName);
                                             var displayName = Path.GetFileNameWithoutExtension(archiveName);
                                             foreach (var node in tree)
                                             {
                                                      node.SourceArchive = displayName;
                                             }
                                             roots.AddRange(tree);
                                    }

                                    var totalCount = unmountedMods.Count + archiveNames.Count;
                                    return (Roots: roots, TotalCount: totalCount);
                           });

                           FileTree.ReplaceRange(result.Roots);
                           RefreshFilteredFileTree();
                           StatusText = $"Loaded {result.TotalCount} archive(s)";
                  }
                  catch (Exception ex)
                  {
                           StatusText = "Error loading archives";
                           Serilog.Log.Error(ex, "Failed to load selected archives");
                  }
                  finally
                  {
                           IsLoading = false;
                  }
         }

         public void LoadAllArchives()
         {
                  FileTree.Clear();
                  Packages.Clear();

                  GameLoading?.Invoke(this, EventArgs.Empty);

                  var unmountedMods = new List<ArchiveViewModel>();

                  foreach (var archive in Archives)
                  {
                           if (archive.IsUnmounted && !string.IsNullOrEmpty(archive.UnmountedPath))
                           {
                                    unmountedMods.Add(archive);
                           }
                  }

                  foreach (var mod in unmountedMods)
                  {
                           StatusText = $"Mounting mod: {mod.Name}...";
                           _pakLoader.RegisterAndMountMod(mod.UnmountedPath!);
                  }

                  StatusText = "Building file tree...";
                  var tree = _pakLoader.GetFileTree();
                  FileTree.ReplaceRange(tree);

                  StatusText = $"Loaded all archives ({Archives.Count} total)";
                  RefreshFilteredFileTree();
         }

         public async Task LoadAllArchivesAsync()
         {
                  IsLoading = true;
                  StatusText = "Loading all archives...";
                  FileTree.Clear();
                  Packages.Clear();

                  GameLoading?.Invoke(this, EventArgs.Empty);

                  await Task.Yield();

                  try
                  {
                           var roots = await Task.Run(() =>
                           {
                                    var unmountedMods = new List<ArchiveViewModel>();

                                    foreach (var archive in Archives)
                                    {
                                             if (archive.IsUnmounted && !string.IsNullOrEmpty(archive.UnmountedPath))
                                             {
                                                      unmountedMods.Add(archive);
                                             }
                                    }

                                    foreach (var mod in unmountedMods)
                                    {
                                             UpdateStatus($"Mounting mod: {mod.Name}...");
                                             _pakLoader.RegisterAndMountMod(mod.UnmountedPath!);
                                    }

                                    UpdateStatus("Building file tree...");
                                    return _pakLoader.GetFileTree();
                           });

                           FileTree.ReplaceRange(roots);
                           RefreshFilteredFileTree();
                           StatusText = $"Loaded all archives ({Archives.Count} total)";
                  }
                  catch (Exception ex)
                  {
                           StatusText = "Error loading archives";
                           Serilog.Log.Error(ex, "Failed to load all archives");
                  }
                  finally
                  {
                           IsLoading = false;
                  }
         }

         private void UpdatePackages(FileNodeViewModel? folder)
         {
                  Packages.Clear();

                  _packageResolutionCts?.Cancel();
                  _packageResolutionCts = new CancellationTokenSource();
                  var token = _packageResolutionCts.Token;

                  if (folder != null)
                  {
                           foreach (var child in folder.Children)
                           {
                                    if (!child.IsDirectory)
                                    {
                                             Packages.Add(child);
                                    }
                           }

                           _ = ResolveFolderAssetsAsync(folder.Children.ToList(), token);
                  }
                  ApplyPackagesViewSettings();
         }

         private async Task ResolveFolderAssetsAsync(List<FileNodeViewModel> items, CancellationToken token)
         {
             if (items.Count == 0) return;

             var provider = _pakLoader.GetProvider();
             if (provider == null) return;

             await Task.Run(() =>
             {
                 foreach (var item in items)
                 {
                     if (token.IsCancellationRequested) break;
                     if (item.IsDirectory || item.IsResolved) continue;

                     try
                     {

                         if (item.Extension != "uasset") continue;

                         var category = TryResolveAssetCategory(provider, item.FullPath);
                         if (category.HasValue)
                         {
                             _dispatcherQueue.TryEnqueue(() =>
                             {
                                 if (token.IsCancellationRequested) return;
                                 ApplyResolvedAssetCategory(item, category.Value);
                             });
                         }
                     }
                     catch
                     {

                     }
                 }
             }, token);
         }

         private bool _isMinimalMode;
         public bool IsMinimalMode
         {
                  get => _isMinimalMode;
                  set => SetProperty(ref _isMinimalMode, value);
         }

         private void OpenSettings()
         {
                  try
                  {
                           SettingsRequested?.Invoke(this, EventArgs.Empty);
                  }
                  catch (Exception ex)
                  {
                           Serilog.Log.Error(ex, "Failed to request settings panel");
                  }
         }

         public CUE4Parse.FileProvider.IFileProvider? GetFileProvider()
         {
                  return _pakLoader.GetProvider();
         }

         public PakLoaderService GetPakLoader()
         {
                  return _pakLoader;
         }

         public void SyncSelectedGameFromSettings()
         {
                  if (!string.IsNullOrEmpty(AppSettings.Instance.LastSelectedGame))
                  {
                           var targetGame = Games.FirstOrDefault(g =>
                                    string.Equals(g.Name, AppSettings.Instance.LastSelectedGame, StringComparison.OrdinalIgnoreCase));

                           if (targetGame != null && targetGame != SelectedGame)
                           {
                                    _selectedGame = targetGame;
                                    OnPropertyChanged(nameof(SelectedGame));
                                    (LoadGameCommand as RelayCommand)?.RaiseCanExecuteChanged();
                           }
                  }
         }

         public async Task LoadGameAsync()
         {

                  var gameConfig = AppSettings.Instance.Games.FirstOrDefault(g =>
                      g.Name == AppSettings.Instance.LastSelectedGame);

                  if (gameConfig != null)
                  {
                           SelectedGame = gameConfig;
                           await LoadGame();
                  }
         }

         public async Task LoadPakFileAsync(string pakFilePath)
         {
                  IsLoading = true;
                  StatusText = $"Loading {Path.GetFileName(pakFilePath)}...";
                  FileTree.Clear();

                  try
                  {
                           await Task.Run(() =>
                           {
                                    var tempGame = new GameConfig
                                    {
                                             Name = Path.GetFileNameWithoutExtension(pakFilePath),
                                             PakPath = Path.GetDirectoryName(pakFilePath) ?? string.Empty,
                                             UsmapPath = null,
                                             AesKey = string.Empty
                                    };

                                    _pakLoader.LoadPakFiles(tempGame);
                                    var tree = _pakLoader.GetFileTree();

                                    var dispatcherQueue = DispatcherQueue.GetForCurrentThread() ??
                                                         Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();

                                    dispatcherQueue?.TryEnqueue(() =>
                     {
                              foreach (var node in tree)
                              {
                                       FileTree.Add(node);
                              }
                              ApplyPackagesViewSettings();
                     });
                           });

                           StatusText = $"Loaded {Path.GetFileName(pakFilePath)} - {_pakLoader.GetFileCount()} files";
                  }
                  catch (Exception ex)
                  {
                           StatusText = $"Error loading {Path.GetFileName(pakFilePath)}";

                           Serilog.Log.Error(ex, "Failed to load pak file {FilePath}", pakFilePath);
                  }
                  finally
                  {
                           IsLoading = false;
                  }
         }

         private void ApplyArchivesViewSettings()
         {

                  var filtered = Archives.AsEnumerable();

                  switch (_archivesFilterOption)
                  {
                           case ArchiveFilterOption.ModsOnly:
                                    filtered = filtered.Where(a => a.IsMod);
                                    break;
                           case ArchiveFilterOption.GameFilesOnly:
                                    filtered = filtered.Where(a => a.IsGameFile);
                                    break;
                  }

                  var q = _archivesSearchText?.Trim();
                  if (!string.IsNullOrEmpty(q))
                  {
                           filtered = filtered.Where(a => a.Name.IndexOf(q, StringComparison.OrdinalIgnoreCase) >= 0);
                  }

                  if (_archivesFilterOption == ArchiveFilterOption.All)
                  {
                           filtered = _archivesSortOption switch
                           {
                                    ArchiveSortOption.NameAsc => filtered.OrderByDescending(a => a.IsMod).ThenBy(a => a.Name),
                                    ArchiveSortOption.NameDesc => filtered.OrderByDescending(a => a.IsMod).ThenByDescending(a => a.Name),
                                    ArchiveSortOption.FileCountAsc => filtered.OrderByDescending(a => a.IsMod).ThenBy(a => a.FileCount),
                                    ArchiveSortOption.FileCountDesc => filtered.OrderByDescending(a => a.IsMod).ThenByDescending(a => a.FileCount),
                                    ArchiveSortOption.SizeAsc => filtered.OrderByDescending(a => a.IsMod).ThenBy(a => a.Size),
                                    ArchiveSortOption.SizeDesc => filtered.OrderByDescending(a => a.IsMod).ThenByDescending(a => a.Size),
                                    _ => filtered.OrderByDescending(a => a.IsMod).ThenBy(a => a.Name)
                           };
                  }
                  else
                  {
                           filtered = _archivesSortOption switch
                           {
                                    ArchiveSortOption.NameAsc => filtered.OrderBy(a => a.Name),
                                    ArchiveSortOption.NameDesc => filtered.OrderByDescending(a => a.Name),
                                    ArchiveSortOption.FileCountAsc => filtered.OrderBy(a => a.FileCount),
                                    ArchiveSortOption.FileCountDesc => filtered.OrderByDescending(a => a.FileCount),
                                    ArchiveSortOption.SizeAsc => filtered.OrderBy(a => a.Size),
                                    ArchiveSortOption.SizeDesc => filtered.OrderByDescending(a => a.Size),
                                    _ => filtered.OrderBy(a => a.Name)
                           };
                  }

                  FilteredArchives.Clear();
                  foreach (var archive in filtered)
                  {
                           FilteredArchives.Add(archive);
                  }

                  OnPropertyChanged(nameof(ModCount));
                  OnPropertyChanged(nameof(GameFileCount));
         }

         private void ApplyPackagesViewSettings()
         {

                  var filtered = Packages.AsEnumerable();

                  var q = _packagesSearchText?.Trim();
                  if (!string.IsNullOrEmpty(q))
                  {
                           filtered = filtered.Where(f => f.Name.IndexOf(q, StringComparison.OrdinalIgnoreCase) >= 0);
                  }

                  filtered = _packagesSortOption switch
                  {
                           PackageSortOption.NameAsc => filtered.OrderBy(f => f.Name),
                           PackageSortOption.NameDesc => filtered.OrderByDescending(f => f.Name),
                           PackageSortOption.SizeAsc => filtered.OrderBy(f => f.Size),
                           PackageSortOption.SizeDesc => filtered.OrderByDescending(f => f.Size),
                           PackageSortOption.ExtensionAsc => filtered.OrderBy(f => f.Extension),
                           PackageSortOption.ExtensionDesc => filtered.OrderByDescending(f => f.Extension),
                           _ => filtered.OrderBy(f => f.Name)
                  };

                  FilteredPackages.Clear();
                  foreach (var file in filtered)
                  {
                           FilteredPackages.Add(file);
                  }
         }

         public void SetFileTree(List<FileNodeViewModel> fileTree)
         {
                  FileTree.Clear();
                  foreach (var node in fileTree)
                  {
                           FileTree.Add(node);
                  }
                  RefreshFilteredFileTree();
         }

         public void SetFileProvider(DefaultFileProvider? provider)
         {

         }
}
