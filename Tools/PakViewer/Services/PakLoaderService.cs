using System;
using System.IO;
using System.Linq;
using CUE4Parse.Compression;
using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Versions;
using CUE4Parse.Encryption.Aes;
using CUE4Parse.UE4.Objects.Core.Misc;
using CUE4Parse_Conversion.Textures.BC;
using PakViewer.Models;
using PakViewer.Settings;
using Serilog;

namespace PakViewer.Services;

public class PakLoaderService
{
         private DefaultFileProvider? _provider;
         private readonly Dictionary<string, FileNodeViewModel> _directoryNodes = new();
         private List<(string original, string renamed)> _disabledModFiles = new();

         public event Action<string>? StatusChanged;

         private void NotifyStatus(string message)
         {
                  try
                  {
                           StatusChanged?.Invoke(message);
                  }
                  catch
                  {
                  }
         }

         public System.Collections.IEnumerable GetMountedArchives()
         {
                  return _provider?.MountedVfs ?? (System.Collections.IEnumerable)new List<object>();
         }

         public void LoadPakFiles(GameConfig gameConfig)
         {
                  try
                  {

                           if (_provider != null)
                           {
                                    Log.Information("Disposing existing provider...");
                                    _provider.Dispose();
                                    _provider = null;

                                    GC.Collect();
                                    GC.WaitForPendingFinalizers();
                           }

                           var resolvedPakPath = ResolvePakPath(gameConfig.PakPath);
                           Log.Information($"Initializing pak provider for {resolvedPakPath}");

                           TryInitializeOodle();

                           TryInitializeZlib();

                           if (!Directory.Exists(resolvedPakPath))
                           {
                                    throw new DirectoryNotFoundException($"Directory not found: {resolvedPakPath}");
                           }

                           var pakFiles = Directory.GetFiles(resolvedPakPath, "*.pak", SearchOption.TopDirectoryOnly);
                           var utocFiles = Directory.GetFiles(resolvedPakPath, "*.utoc", SearchOption.TopDirectoryOnly);
                           Log.Information($"Found {pakFiles.Length} .pak files and {utocFiles.Length} .utoc files in base game directory");

                           var ueVersion = ParseUeVersion(gameConfig.UeVersion);
                           Log.Information($"Using Unreal Engine version: {gameConfig.UeVersion} ({ueVersion})");

                           _provider = new DefaultFileProvider(resolvedPakPath, SearchOption.TopDirectoryOnly,
                               new VersionContainer(ueVersion), StringComparer.OrdinalIgnoreCase);

                           if (!string.IsNullOrWhiteSpace(gameConfig.UsmapPath) && File.Exists(gameConfig.UsmapPath))
                           {
                                    Log.Information($"Loading mappings from {gameConfig.UsmapPath}");
                                    _provider.MappingsContainer = new FileUsmapTypeMappingsProvider(gameConfig.UsmapPath);
                           }
                           else
                           {
                                    Log.Information("No .usmap configured.");
                           }

                           Log.Information("Initializing provider...");
                           _provider.Initialize();

                           Log.Information($"Discovered {_provider.Files.Count} files from {resolvedPakPath}");
                           Log.Information($"Mounted archives: {_provider.MountedVfs.Count}");
                           Log.Information($"Unloaded VFS count: {_provider.UnloadedVfs.Count}");

                           if (!string.IsNullOrWhiteSpace(gameConfig.AesKey))
                           {
                                    Log.Information("Submitting AES key for encrypted archives...");
                                    var fixedKey = FixKey(gameConfig.AesKey);
                                    if (string.IsNullOrEmpty(fixedKey))
                                    {
                                             Log.Warning("AES key is invalid after normalization, skipping key submission");
                                    }
                                    else
                                    {
                                             Log.Information($"Submitting AES key for encrypted archives (normalized: {fixedKey})");
                                             _provider.SubmitKey(new FGuid(), new FAesKey(fixedKey));
                                    }
                           }

                           if (_provider.UnloadedVfs.Count > 0)
                           {
                                    Log.Information("Attempting to mount unloaded archives...");
                                    var mountedCount = _provider.Mount();
                                    Log.Information($"Successfully mounted {mountedCount} additional archives");
                           }

                           foreach (var vfs in _provider.MountedVfs)
                           {
                                    Log.Information($"✓ Mounted: {vfs.Name} ({vfs.FileCount} files)");
                           }

                           if (_provider.UnloadedVfs.Count > 0)
                           {
                                    Log.Warning($"Still have {_provider.UnloadedVfs.Count} unloaded archives:");
                                    foreach (var vfs in _provider.UnloadedVfs.Take(10))
                                    {
                                             Log.Warning($"  ✗ {vfs.Name} - IsEncrypted: {vfs.IsEncrypted}");
                                    }
                                    if (_provider.UnloadedVfs.Count > 10)
                                    {
                                             Log.Warning($"  ... and {_provider.UnloadedVfs.Count - 10} more");
                                    }
                           }

                           if (_provider.MountedVfs.Count == 0 && (pakFiles.Length > 0 || utocFiles.Length > 0))
                           {
                                    Log.Warning("No archives were mounted! This usually means:");
                                    Log.Warning("1. The files are encrypted and need an AES key");
                                    Log.Warning("2. The UE version is incorrect");
                                    Log.Warning("3. The files are corrupted");
                           }
                  }
                  catch (Exception ex)
                  {
                           Log.Error(ex, "Failed to load PAK files");
                           throw new Exception($"Failed to load PAK files: {ex.Message}", ex);
                  }
         }

         private static string FixKey(string key)
         {
                  if (string.IsNullOrEmpty(key))
                      return string.Empty;

                  var trimmed = key.Replace(" ", "").Replace("-", "").Trim();
                  if (trimmed.Length == 0)
                      return string.Empty;

                  if (trimmed.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
                      trimmed = trimmed[2..];

                  if (trimmed.Length != 64)
                      return string.Empty;

                  foreach (char c in trimmed)
                  {
                      if (!Uri.IsHexDigit(c))
                          return string.Empty;
                  }

                  return "0x" + trimmed.ToUpperInvariant();
         }

        public void ClearPackageCache()
        {
            if (_provider == null) return;

            try
            {

                var providerType = _provider.GetType();

                var cacheField = providerType.GetProperty("PackageCache",
                    System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);

                if (cacheField != null)
                {
                    var cache = cacheField.GetValue(_provider);
                    if (cache is System.Collections.IDictionary dict)
                    {
                        var count = dict.Count;
                        dict.Clear();
                        Log.Information($"Cleared {count} cached packages from provider");
                    }
                    else if (cache is System.Collections.IList list)
                    {
                        var count = list.Count;
                        list.Clear();
                        Log.Information($"Cleared {count} cached items from provider");
                    }
                }

                GC.Collect(GC.MaxGeneration, GCCollectionMode.Optimized);
                GC.WaitForPendingFinalizers();
            }
            catch (Exception ex)
            {
                Log.Debug(ex, "Could not clear provider package cache (non-critical)");
            }
        }

        public string ResolvePakPath(string basePath)
        {
                 if (string.IsNullOrWhiteSpace(basePath))
                 {
                          return basePath;
                 }

                 var trimmed = basePath.Trim();
                 if (Directory.Exists(trimmed) && HasPakFiles(trimmed))
                 {
                          return trimmed;
                 }

                 var candidates = new[]
                 {
                          Path.Combine(trimmed, "Content", "Paks"),
                          Path.Combine(trimmed, "Content", "Paks", "Windows"),
                          Path.Combine(trimmed, "Content", "Paks", "WindowsNoEditor"),
                          Path.Combine(trimmed, "Content", "Paks", "Win64"),
                          Path.Combine(trimmed, "Paks")
                 };

                 foreach (var candidate in candidates)
                 {
                          if (Directory.Exists(candidate) && HasPakFiles(candidate))
                          {
                                   return candidate;
                          }
                 }

                 var found = FindPakFolder(trimmed, 4);
                 return found ?? trimmed;
        }

        private static bool HasPakFiles(string directory)
        {
                 return Directory.EnumerateFiles(directory, "*.pak", SearchOption.TopDirectoryOnly).Any() ||
                        Directory.EnumerateFiles(directory, "*.utoc", SearchOption.TopDirectoryOnly).Any();
        }

        private static string? FindPakFolder(string root, int maxDepth)
        {
                 if (!Directory.Exists(root))
                 {
                          return null;
                 }

                 var queue = new Queue<(string path, int depth)>();
                 queue.Enqueue((root, 0));

                 while (queue.Count > 0)
                 {
                          var (path, depth) = queue.Dequeue();
                          if (HasPakFiles(path))
                          {
                                   return path;
                          }

                          if (depth >= maxDepth)
                          {
                                   continue;
                          }

                          try
                          {
                                   foreach (var dir in Directory.EnumerateDirectories(path))
                                   {
                                            queue.Enqueue((dir, depth + 1));
                                   }
                          }
                          catch
                          {
                          }
                 }

                 return null;
        }

         public List<FileNodeViewModel> GetFileTree()
         {
                  if (_provider == null)
                           return new List<FileNodeViewModel>();

                  _directoryNodes.Clear();
                  var rootNodes = new List<FileNodeViewModel>();

                  var modArchiveNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                  var modFolderNames = AppSettings.Instance.ModFolders;

                  foreach (var vfs in _provider.MountedVfs)
                  {
                           var name = vfs.Name;

                           try
                           {

                                    dynamic dynVfs = vfs;
                                    string? vfsPath = null;
                                    try { vfsPath = dynVfs.Path; } catch { }

                                    if (!string.IsNullOrEmpty(vfsPath))
                                    {

                                             var pathLower = vfsPath.ToLowerInvariant().Replace('\\', '/');
                                             foreach (var modFolder in modFolderNames)
                                             {
                                                      if (pathLower.Contains($"/{modFolder}/") || pathLower.Contains($"\\{modFolder}\\"))
                                                      {
                                                               modArchiveNames.Add(name);
                                                               break;
                                                      }
                                             }
                                    }
                           }
                           catch {  }

                           if (!modArchiveNames.Contains(name))
                           {

                                    if (name.StartsWith("pakchunk", StringComparison.OrdinalIgnoreCase) ||
                                        name.StartsWith("global", StringComparison.OrdinalIgnoreCase))
                                    {
                                             continue;
                                    }

                                    if (name.Contains("_P.", StringComparison.OrdinalIgnoreCase) ||
                                        name.Contains("-P.", StringComparison.OrdinalIgnoreCase) ||
                                        System.Text.RegularExpressions.Regex.IsMatch(name, @"^\d+[-_]") ||
                                        System.Text.RegularExpressions.Regex.IsMatch(name, @"[-_]\d{10,}_P") ||
                                        System.Text.RegularExpressions.Regex.IsMatch(name, @"^[A-Z]\d+\s"))
                                    {
                                             modArchiveNames.Add(name);
                                    }
                           }
                  }

                  foreach (var kvp in _provider.Files.OrderBy(x => x.Key))
                  {
                           var file = kvp.Key;
                           var gameFile = kvp.Value;
                           var size = gameFile.Size;

                           if (file.EndsWith(".ushaderbytecode", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".ushadercode", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".upipelinecache", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".uexp", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".ubulk", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".uptnl", StringComparison.OrdinalIgnoreCase))
                                    continue;

                           bool isFromMod = false;
                           try
                           {
                                    dynamic dynFile = gameFile;
                                    if (dynFile.Vfs != null)
                                    {
                                             string vfsName = dynFile.Vfs.Name;
                                             isFromMod = modArchiveNames.Contains(vfsName);
                                    }
                           }
                           catch {  }

                           var parts = file.Split('/', StringSplitOptions.RemoveEmptyEntries);
                           FileNodeViewModel? parentNode = null;
                           string currentPath = "";

                           for (int i = 0; i < parts.Length; i++)
                           {
                                    var part = parts[i];
                                    currentPath = string.IsNullOrEmpty(currentPath) ? part : $"{currentPath}/{part}";
                                    var isFile = i == parts.Length - 1;

                                    if (isFile)
                                    {

                                             var fileNode = new FileNodeViewModel(part, false, currentPath, parentNode) { Size = size };
                                            if (parentNode != null)
                                            {

                                                if (!parentNode.Children.Any(c => string.Equals(c.Name, part, StringComparison.OrdinalIgnoreCase)))
                                                {
                                                    parentNode.Children.Add(fileNode);
                                                }

                                                if (isFromMod)
                                                {
                                                    MarkParentFoldersAsMod(parentNode);
                                                }
                                            }
                                            else
                                            {
                                                if (!rootNodes.Any(n => string.Equals(n.Name, part, StringComparison.OrdinalIgnoreCase)))
                                                {
                                                    rootNodes.Add(fileNode);
                                                }
                                            }
                                    }
                                    else
                                    {

                                             if (!_directoryNodes.TryGetValue(currentPath, out var dirNode))
                                             {
                                                      dirNode = new FileNodeViewModel(part, true, currentPath, parentNode);
                                                      _directoryNodes[currentPath] = dirNode;

                                                      if (parentNode != null)
                                                      {
                                                               parentNode.Children.Add(dirNode);
                                                      }
                                                      else
                                                      {
                                                               rootNodes.Add(dirNode);
                                                      }
                                             }
                                             parentNode = dirNode;
                                    }
                           }
                  }

                  MarkSkeletalMeshes(rootNodes);

                  SortFileTree(rootNodes);

                  ApplyFolderCounts(rootNodes);

                  return rootNodes;
         }

         private void SortFileTree(List<FileNodeViewModel> nodes)
         {

                  nodes.Sort((a, b) =>
                  {

                           if (a.IsDirectory != b.IsDirectory)
                                    return a.IsDirectory ? -1 : 1;

                           return StringComparer.OrdinalIgnoreCase.Compare(a.Name, b.Name);
                  });

                  foreach (var node in nodes)
                  {
                           if (node.Children.Count > 0)
                           {
                                    var childrenList = node.Children.ToList();
                                    SortFileTree(childrenList);

                                    node.Children.Clear();
                                    foreach (var child in childrenList)
                                    {
                                             node.Children.Add(child);
                                    }
                           }
                  }
         }

         private void CalculateFolderCounts(FileNodeViewModel node)
         {
                  if (!node.IsDirectory) return;

                  int count = 0;
                  foreach (var child in node.Children)
                  {
                           if (child.IsDirectory)
                           {

                                    CalculateFolderCounts(child);
                           }
                           else
                           {

                                    count++;
                           }
                  }

                  node.FileCount = count;
         }

         private void ApplyFolderCounts(List<FileNodeViewModel> rootNodes)
         {
                  foreach (var node in rootNodes)
                  {
                           CalculateFolderCounts(node);
                  }
         }

         private void MarkParentFoldersAsMod(FileNodeViewModel folder)
         {
                  var current = folder;
                  while (current != null)
                  {
                           if (current.IsDirectory && !current.IsModFolder)
                           {
                                    current.IsModFolder = true;
                           }
                           current = current.Parent;
                  }
         }

         private void MarkSkeletalMeshes(List<FileNodeViewModel> nodes)
         {

                  var allFiles = new List<FileNodeViewModel>();
                  CollectAllFileNodes(nodes, allFiles);

                  var allFileNames = new HashSet<string>(allFiles.Select(f => f.Name), StringComparer.OrdinalIgnoreCase);

                  foreach (var file in allFiles)
                  {
                           if (file.IsDirectory) continue;

                           if (!file.Name.EndsWith(".uasset", StringComparison.OrdinalIgnoreCase))
                                    continue;

                           if (file.IsPhysicsAsset || file.IsTexture || file.IsMaterial)
                                    continue;

                           if (file.Name.Contains("_PhysicsAsset", StringComparison.OrdinalIgnoreCase) ||
                               file.Name.Contains("COL", StringComparison.OrdinalIgnoreCase) ||
                               file.Name.Contains("Collision", StringComparison.OrdinalIgnoreCase) ||
                               file.Name.Contains("_Anim", StringComparison.OrdinalIgnoreCase) ||
                               file.Name.Contains("Montage", StringComparison.OrdinalIgnoreCase) ||
                               file.Name.Contains("Sequence", StringComparison.OrdinalIgnoreCase))
                                    continue;

                           var physicsAssetName = file.Name.Replace(".uasset", "_PhysicsAsset.uasset", StringComparison.OrdinalIgnoreCase);
                           if (allFileNames.Contains(physicsAssetName))
                           {
                                    file.IsSkeletalMesh = true;
                           }
                  }
         }

         private void CollectAllFileNodes(List<FileNodeViewModel> nodes, List<FileNodeViewModel> result)
         {
                  foreach (var node in nodes)
                  {
                           if (!node.IsDirectory)
                           {
                                    result.Add(node);
                           }

                           if (node.Children.Count > 0)
                           {
                                    CollectAllFileNodes(node.Children.ToList(), result);
                           }
                  }
         }

         public int GetFileCount()
         {
                  return _provider?.Files.Count ?? 0;
         }

         public DefaultFileProvider? GetProvider()
         {
                  return _provider;
         }

         public void LoadModOnly(string modPakPath, GameConfig gameConfig)
         {
                  try
                  {
                           Log.Information($"[MOD-ONLY MODE] Loading: {modPakPath}");

                           TryInitializeOodle();

                           TryInitializeZlib();

                           if (!File.Exists(modPakPath))
                           {
                                    throw new FileNotFoundException($"Mod pak not found: {modPakPath}");
                           }

                           if (!Directory.Exists(gameConfig.PakPath))
                           {
                                    throw new DirectoryNotFoundException($"Game directory not found: {gameConfig.PakPath}");
                           }

                           Log.Information($"Loading base game paks (EXCLUDING other mods): {gameConfig.PakPath}");

                           var ueVersion = ParseUeVersion(gameConfig.UeVersion);
                           Log.Information($"Using Unreal Engine version: {gameConfig.UeVersion} ({ueVersion})");

                           _provider = new DefaultFileProvider(gameConfig.PakPath, SearchOption.TopDirectoryOnly,
                               new VersionContainer(ueVersion), StringComparer.OrdinalIgnoreCase);

                           if (!string.IsNullOrWhiteSpace(gameConfig.UsmapPath) && File.Exists(gameConfig.UsmapPath))
                           {
                                    Log.Information($"Loading mappings from {gameConfig.UsmapPath}");
                                    _provider.MappingsContainer = new FileUsmapTypeMappingsProvider(gameConfig.UsmapPath);
                           }

                           Log.Information("Initializing provider (base game only, for IoStore global data)...");
                           _provider.Initialize();

                           if (!string.IsNullOrWhiteSpace(gameConfig.AesKey))
                           {
                                    var fixedKey = FixKey(gameConfig.AesKey);
                                    if (string.IsNullOrEmpty(fixedKey))
                                    {
                                             Log.Warning("AES key is invalid after normalization, skipping key submission");
                                    }
                                    else
                                    {
                                             Log.Information($"Submitting AES key for encrypted archives (normalized: {fixedKey})");
                                             _provider.SubmitKey(new FGuid(), new FAesKey(fixedKey));
                                    }
                           }

                           if (_provider.UnloadedVfs.Count > 0)
                           {
                                    Log.Information($"Mounting {_provider.UnloadedVfs.Count} base game archives...");
                                    var baseMountedCount = _provider.Mount();
                                    Log.Information($"Mounted {baseMountedCount} base game archives");
                           }

                           Log.Information($"Registering specific mod: {Path.GetFileName(modPakPath)}");
                           var modDir = Path.GetDirectoryName(modPakPath);
                           if (!string.IsNullOrEmpty(modDir))
                           {

                                    var modBaseName = Path.GetFileNameWithoutExtension(modPakPath);
                                    bool isDisabledMod = modPakPath.EndsWith("-x", StringComparison.OrdinalIgnoreCase);

                                    if (isDisabledMod)
                                    {

                                             modBaseName = modBaseName.Replace(".pak", "").Replace(".utoc", "").Replace(".ucas", "");
                                             if (modBaseName.EndsWith("-x"))
                                             {
                                                      modBaseName = modBaseName.Substring(0, modBaseName.Length - 2);
                                             }
                                             Log.Information($"Detected disabled mod, base name: {modBaseName}");
                                    }

                                    var modPak = Path.Combine(modDir, $"{modBaseName}.pak");
                                    var modUtoc = Path.Combine(modDir, $"{modBaseName}.utoc");
                                    var modUcas = Path.Combine(modDir, $"{modBaseName}.ucas");

                                    var modPakDisabled = Path.Combine(modDir, $"{modBaseName}.pak-x");
                                    var modUtocDisabled = Path.Combine(modDir, $"{modBaseName}.utoc-x");
                                    var modUcasDisabled = Path.Combine(modDir, $"{modBaseName}.ucas-x");

                                    var renamedFiles = new List<(string original, string renamed)>();

                                    try
                                    {

                                             if (isDisabledMod)
                                             {
                                                      if (File.Exists(modPakDisabled) && !File.Exists(modPak))
                                                      {
                                                               File.Move(modPakDisabled, modPak);
                                                               renamedFiles.Add((modPakDisabled, modPak));
                                                               Log.Information($"  Temporarily enabled: {Path.GetFileName(modPak)}");
                                                      }
                                                      if (File.Exists(modUtocDisabled) && !File.Exists(modUtoc))
                                                      {
                                                               File.Move(modUtocDisabled, modUtoc);
                                                               renamedFiles.Add((modUtocDisabled, modUtoc));
                                                               Log.Information($"  Temporarily enabled: {Path.GetFileName(modUtoc)}");
                                                      }
                                                      if (File.Exists(modUcasDisabled) && !File.Exists(modUcas))
                                                      {
                                                               File.Move(modUcasDisabled, modUcas);
                                                               renamedFiles.Add((modUcasDisabled, modUcas));
                                                               Log.Information($"  Temporarily enabled: {Path.GetFileName(modUcas)}");
                                                      }
                                             }

                                             int modFilesRegistered = 0;
                                             if (File.Exists(modPak))
                                             {
                                                      _provider.RegisterVfs(modPak);
                                                      modFilesRegistered++;
                                                      Log.Information($"  Registered: {Path.GetFileName(modPak)}");
                                             }

                                             var hasUtoc = File.Exists(modUtoc);
                                             var hasUcas = File.Exists(modUcas);
                                             if (hasUtoc && !hasUcas)
                                             {
                                                      Log.Warning($"  Found IoStore container {Path.GetFileName(modUtoc)} but missing {Path.GetFileName(modUcas)}; skipping .utoc mount");
                                             }
                                             else if (hasUtoc && hasUcas)
                                             {
                                                      _provider.RegisterVfs(modUcas);
                                                      modFilesRegistered++;
                                                      Log.Information($"  Registered: {Path.GetFileName(modUcas)}");

                                                      _provider.RegisterVfs(modUtoc);
                                                      modFilesRegistered++;
                                                      Log.Information($"  Registered: {Path.GetFileName(modUtoc)}");
                                             }

                                             if (modFilesRegistered > 0)
                                             {

                                                      var modMountedCount = _provider.Mount();
                                                      Log.Information($"✓ Mounted {modMountedCount} mod archives");
                                             }

                                             _disabledModFiles = renamedFiles;
                                    }
                                    catch (Exception ex)
                                    {
                                             Log.Error(ex, "Error handling mod files");

                                             RestoreDisabledModFiles(renamedFiles);
                                             throw;
                                    }
                           }

                           Log.Information($"✓ Provider initialized with {_provider.Files.Count} total files");
                           Log.Information($"✓ Now filtering to show ONLY files from: {Path.GetFileName(modPakPath)}");
                  }
                  catch (Exception ex)
                  {
                           Log.Error(ex, "Failed to load mod-only");
                           throw;
                  }
         }

         public List<FileNodeViewModel> GetModFileTree(string modPakPath)
         {
                  if (_provider == null)
                           return new List<FileNodeViewModel>();

                  var modPakName = Path.GetFileNameWithoutExtension(modPakPath);

                  if (modPakName.EndsWith("-x", StringComparison.OrdinalIgnoreCase))
                           modPakName = modPakName[..^2];

                  modPakName = modPakName.Replace(".pak", "").Replace(".utoc", "").Replace(".ucas", "");

                  var modFiles = new Dictionary<string, long>(StringComparer.OrdinalIgnoreCase);

                  if (_provider is CUE4Parse.FileProvider.DefaultFileProvider dfp)
                  {

                           var modVfs = dfp.MountedVfs.FirstOrDefault(vfs =>
                               vfs.Name.Contains($"{modPakName}.utoc", StringComparison.OrdinalIgnoreCase));

                           if (modVfs == null)
                           {
                                    modVfs = dfp.MountedVfs.FirstOrDefault(vfs =>
                                        vfs.Name.Contains(modPakName, StringComparison.OrdinalIgnoreCase));
                           }

                           if (modVfs != null)
                           {
                                    Log.Information($"Found VFS: {modVfs.Name} with {modVfs.FileCount} files");

                                    try
                                    {
                                             var filesProperty = modVfs.GetType().GetProperty("Files");
                                             if (filesProperty != null)
                                             {
                                                      var files = filesProperty.GetValue(modVfs);
                                                      if (files is System.Collections.IDictionary fileDict)
                                                      {
                                                               Log.Information($"Accessing Files property via reflection, count: {fileDict.Count}");

                                                               foreach (System.Collections.DictionaryEntry entry in fileDict)
                                                               {
                                                                        var filePath = entry.Key?.ToString();
                                                                        if (!string.IsNullOrEmpty(filePath))
                                                                        {

                                                                                 long fileSize = 0;
                                                                                 try
                                                                                 {
                                                                                          if (_provider.Files.TryGetValue(filePath, out var gameFile))
                                                                                          {
                                                                                                   fileSize = gameFile.Size;
                                                                                          }
                                                                                 }
                                                                                 catch { }
                                                                                 modFiles[filePath] = fileSize;
                                                                        }
                                                               }
                                                      }
                                             }
                                    }
                                    catch (Exception ex)
                                    {
                                             Log.Error(ex, $"Failed to get files from mod VFS: {modVfs.Name}");
                                    }
                           }
                           else
                           {
                                    Log.Warning($"VFS reader not found for mod: {modPakName}");
                           }
                  }

                  if (modFiles.Count == 0)
                  {
                           Log.Warning($"No files found for mod: {modPakName}");
                           return new List<FileNodeViewModel>();
                  }

                  _directoryNodes.Clear();
                  var rootNodes = new List<FileNodeViewModel>();

                  foreach (var kvp in modFiles.OrderBy(x => x.Key))
                  {
                           var file = kvp.Key;
                           var size = kvp.Value;

                           if (file.EndsWith(".ushaderbytecode", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".ushadercode", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".upipelinecache", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".uexp", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".ubulk", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".uptnl", StringComparison.OrdinalIgnoreCase))
                                    continue;

                           var parts = file.Split('/', StringSplitOptions.RemoveEmptyEntries);
                           FileNodeViewModel? parentNode = null;
                           string currentPath = "";

                           for (int i = 0; i < parts.Length; i++)
                           {
                                    var part = parts[i];
                                    currentPath = string.IsNullOrEmpty(currentPath) ? part : $"{currentPath}/{part}";
                                    var isFile = i == parts.Length - 1;

                                    if (isFile)
                                    {
                                             var fileNode = new FileNodeViewModel(part, false, currentPath, parentNode) { Size = size };
                                             if (parentNode != null)
                                             {
                                                      if (!parentNode.Children.Any(c => string.Equals(c.FullPath, currentPath, StringComparison.OrdinalIgnoreCase)))
                                                      {
                                                               parentNode.Children.Add(fileNode);
                                                      }
                                             }
                                             else
                                             {
                                                      if (!rootNodes.Any(n => string.Equals(n.FullPath, currentPath, StringComparison.OrdinalIgnoreCase)))
                                                      {
                                                               rootNodes.Add(fileNode);
                                                      }
                                             }
                                    }
                                    else
                                    {
                                             if (!_directoryNodes.TryGetValue(currentPath, out var dirNode))
                                             {
                                                      dirNode = new FileNodeViewModel(part, true, currentPath, parentNode);
                                                      _directoryNodes[currentPath] = dirNode;

                                                      if (parentNode != null)
                                                      {
                                                               if (!parentNode.Children.Any(c => string.Equals(c.FullPath, currentPath, StringComparison.OrdinalIgnoreCase)))
                                                               {
                                                                        parentNode.Children.Add(dirNode);
                                                               }
                                                      }
                                                      else
                                                      {
                                                               if (!rootNodes.Any(n => string.Equals(n.FullPath, currentPath, StringComparison.OrdinalIgnoreCase)))
                                                               {
                                                                        rootNodes.Add(dirNode);
                                                               }
                                                      }
                                             }
                                             parentNode = dirNode;
                                    }
                           }
                  }

                  MarkSkeletalMeshes(rootNodes);

                  SortFileTree(rootNodes);

                  ApplyFolderCounts(rootNodes);

                  return rootNodes;
         }

         public List<FileNodeViewModel> GetArchiveFileTree(string archiveName)
         {
                  if (_provider == null)
                           return new List<FileNodeViewModel>();

                  var archiveFiles = new List<string>();

                  if (_provider is CUE4Parse.FileProvider.DefaultFileProvider dfp)
                  {
                           var vfs = dfp.MountedVfs.FirstOrDefault(x => x.Name.Equals(archiveName, StringComparison.OrdinalIgnoreCase));

                           if (vfs != null)
                           {
                                    try
                                    {

                                             var filesProperty = vfs.GetType().GetProperty("Files");
                                             if (filesProperty != null)
                                             {
                                                      var files = filesProperty.GetValue(vfs);
                                                      if (files is System.Collections.IDictionary fileDict)
                                                      {
                                                               foreach (var key in fileDict.Keys)
                                                               {
                                                                        var filePath = key.ToString();
                                                                        if (!string.IsNullOrEmpty(filePath))
                                                                        {
                                                                                 archiveFiles.Add(filePath);
                                                                        }
                                                               }
                                                      }
                                             }
                                    }
                                    catch (Exception ex)
                                    {
                                             Log.Error(ex, $"Failed to get files from archive {archiveName}");
                                    }
                           }
                  }

                  if (archiveFiles.Count == 0)
                           return new List<FileNodeViewModel>();

                  _directoryNodes.Clear();
                  var rootNodes = new List<FileNodeViewModel>();

                  foreach (var file in archiveFiles.OrderBy(x => x))
                  {

                           if (file.EndsWith(".ushaderbytecode", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".ushadercode", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".upipelinecache", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".uexp", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".ubulk", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".uptnl", StringComparison.OrdinalIgnoreCase))
                                    continue;

                           var parts = file.Split('/', StringSplitOptions.RemoveEmptyEntries);
                           FileNodeViewModel? parentNode = null;
                           string currentPath = "";

                           for (int i = 0; i < parts.Length; i++)
                           {
                                    var part = parts[i];
                                    currentPath = string.IsNullOrEmpty(currentPath) ? part : $"{currentPath}/{part}";
                                    var isFile = i == parts.Length - 1;

                                    if (isFile)
                                    {
                                             var fileNode = new FileNodeViewModel(part, false, currentPath, parentNode);
                                             if (_provider.Files.TryGetValue(currentPath, out var gameFile))
                                             {
                                                      fileNode.Size = gameFile.Size;
                                             }
                                             if (parentNode != null)
                                             {
                                                      if (!parentNode.Children.Any(c => string.Equals(c.FullPath, currentPath, StringComparison.OrdinalIgnoreCase)))
                                                      {
                                                               parentNode.Children.Add(fileNode);
                                                      }
                                             }
                                             else
                                             {
                                                      if (!rootNodes.Any(n => string.Equals(n.FullPath, currentPath, StringComparison.OrdinalIgnoreCase)))
                                                      {
                                                               rootNodes.Add(fileNode);
                                                      }
                                             }
                                    }
                                    else
                                    {
                                             if (!_directoryNodes.TryGetValue(currentPath, out var dirNode))
                                             {
                                                      dirNode = new FileNodeViewModel(part, true, currentPath, parentNode);
                                                      _directoryNodes[currentPath] = dirNode;

                                                      if (parentNode != null)
                                                      {
                                                               if (!parentNode.Children.Any(c => string.Equals(c.FullPath, currentPath, StringComparison.OrdinalIgnoreCase)))
                                                               {
                                                                        parentNode.Children.Add(dirNode);
                                                               }
                                                      }
                                                      else
                                                      {
                                                               if (!rootNodes.Any(n => string.Equals(n.FullPath, currentPath, StringComparison.OrdinalIgnoreCase)))
                                                               {
                                                                        rootNodes.Add(dirNode);
                                                               }
                                                      }
                                             }
                                             parentNode = dirNode;
                                    }
                           }
                  }

                  MarkSkeletalMeshes(rootNodes);

                  SortFileTree(rootNodes);

                  ApplyFolderCounts(rootNodes);

                  return rootNodes;
         }

         public List<FileNodeViewModel> GetCombinedArchivesFileTree(IEnumerable<string> archiveNames)
         {
                  if (_provider == null)
                           return new List<FileNodeViewModel>();

                  var selectedArchives = new HashSet<string>(archiveNames, StringComparer.OrdinalIgnoreCase);

                  var allFiles = new List<(string path, long size)>();

                  foreach (var kvp in _provider.Files)
                  {
                           var filePath = kvp.Key;
                           var gameFile = kvp.Value;

                           if (filePath.EndsWith(".uexp") || filePath.EndsWith(".ubulk") || filePath.EndsWith(".uptnl"))
                                    continue;

                           try
                           {
                                    string? vfsName = null;

                                    dynamic dynFile = gameFile;

                                    if (dynFile.Vfs != null)
                                    {
                                             vfsName = dynFile.Vfs.Name;
                                    }

                                    if (!string.IsNullOrEmpty(vfsName) && selectedArchives.Contains(vfsName))
                                    {
                                             allFiles.Add((filePath, gameFile.Size));
                                    }
                           }
                           catch
                           {

                                    continue;
                           }
                  }

                  if (allFiles.Count == 0)
                           return new List<FileNodeViewModel>();

                  _directoryNodes.Clear();
                  var rootNodes = new List<FileNodeViewModel>();

                  foreach (var entry in allFiles.OrderBy(x => x.path))
                  {
                           var file = entry.path;
                           var size = entry.size;

                           if (file.EndsWith(".ushaderbytecode", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".ushadercode", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".upipelinecache", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".uexp", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".ubulk", StringComparison.OrdinalIgnoreCase) ||
                               file.EndsWith(".uptnl", StringComparison.OrdinalIgnoreCase))
                                    continue;

                           var parts = file.Split('/', StringSplitOptions.RemoveEmptyEntries);
                           FileNodeViewModel? parentNode = null;
                           string currentPath = "";

                           for (int i = 0; i < parts.Length; i++)
                           {
                                    var part = parts[i];
                                    currentPath = string.IsNullOrEmpty(currentPath) ? part : $"{currentPath}/{part}";
                                    var isFile = i == parts.Length - 1;

                                    if (isFile)
                                    {
                                             var fileNode = new FileNodeViewModel(part, false, currentPath, parentNode) { Size = size };
                                             if (parentNode != null)
                                             {
                                                      if (!parentNode.Children.Any(c => string.Equals(c.FullPath, currentPath, StringComparison.OrdinalIgnoreCase)))
                                                      {
                                                               parentNode.Children.Add(fileNode);
                                                      }
                                             }
                                             else
                                             {
                                                      if (!rootNodes.Any(n => string.Equals(n.FullPath, currentPath, StringComparison.OrdinalIgnoreCase)))
                                                      {
                                                               rootNodes.Add(fileNode);
                                                      }
                                             }
                                    }
                                    else
                                    {
                                             if (!_directoryNodes.TryGetValue(currentPath, out var dirNode))
                                             {
                                                      dirNode = new FileNodeViewModel(part, true, currentPath, parentNode);
                                                      _directoryNodes[currentPath] = dirNode;

                                                      if (parentNode != null)
                                                      {
                                                               if (!parentNode.Children.Any(c => string.Equals(c.FullPath, currentPath, StringComparison.OrdinalIgnoreCase)))
                                                               {
                                                                        parentNode.Children.Add(dirNode);
                                                               }
                                                      }
                                                      else
                                                      {
                                                               if (!rootNodes.Any(n => string.Equals(n.FullPath, currentPath, StringComparison.OrdinalIgnoreCase)))
                                                               {
                                                                        rootNodes.Add(dirNode);
                                                               }
                                                      }
                                             }
                                             parentNode = dirNode;
                                    }
                           }
                  }

                  MarkSkeletalMeshes(rootNodes);

                  SortFileTree(rootNodes);

                  ApplyFolderCounts(rootNodes);

                  return rootNodes;
         }

         public void UnloadProvider()
         {
                  _provider?.Dispose();
                  _provider = null;
                  _directoryNodes.Clear();

                  RestoreDisabledModFiles(_disabledModFiles);
                  _disabledModFiles.Clear();

                  Log.Information("Provider unloaded and disposed");
         }

         private void RestoreDisabledModFiles(List<(string original, string renamed)> files)
         {
                  foreach (var (original, renamed) in files)
                  {
                           try
                           {
                                    if (File.Exists(renamed) && !File.Exists(original))
                                    {
                                             File.Move(renamed, original);
                                             Log.Information($"  Restored: {Path.GetFileName(original)}");
                                    }
                           }
                           catch (Exception ex)
                           {
                                    Log.Warning($"Failed to restore {Path.GetFileName(original)}: {ex.Message}");
                           }
                  }
         }

         private void TryInitializeOodle()
         {
                  try
                  {

                           if (CUE4Parse.Utils.CUE4ParseNatives.IsFeatureAvailable("Oodle\0"u8))
                           {
                                    try
                                    {
                                             Log.Information("Using native Oodle decompression");
                                             OodleHelper.Initialize();
                                             Log.Information("Oodle decompression initialized successfully (native)");
                                             return;
                                    }
                                    catch (Exception ex)
                                    {
                                             Log.Warning($"Native Oodle initialization failed: {ex.Message}. Trying other methods...");

                                    }
                           }

                           var possiblePaths = new[]
                           {
                                    Path.Combine(AppDomain.CurrentDomain.BaseDirectory, OodleHelper.OODLE_NAME_CURRENT),
                                    Path.Combine(AppDomain.CurrentDomain.BaseDirectory, OodleHelper.OODLE_NAME_OLD),
                                    Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "oo2core_8_win64.dll"),
                                    Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "oo2core_win64.dll")
                           };

                           foreach (var path in possiblePaths)
                           {
                                    if (File.Exists(path))
                                    {
                                             Log.Information($"Found Oodle library at: {path}");
                                             OodleHelper.Initialize(path);
                                             Log.Information("Oodle decompression initialized successfully");
                                             return;
                                    }
                           }

                           Log.Information("Oodle DLL not found locally. Attempting to download...");
                           NotifyStatus("Compression library wasn't found, attempting to download it...");
                           string? downloadedOodlePath = null;
                           if (OodleHelper.DownloadOodleDll(ref downloadedOodlePath) && !string.IsNullOrWhiteSpace(downloadedOodlePath))
                           {
                                    Log.Information("Oodle DLL downloaded successfully");
                                    NotifyStatus("Compression library downloaded. Continuing...");
                                    try
                                    {

                                             OodleHelper.Initialize(downloadedOodlePath);
                                             Log.Information("Oodle decompression initialized successfully (downloaded)");
                                    }
                                    catch (Exception ex)
                                    {
                                             Log.Warning(ex, $"Failed to initialize downloaded Oodle DLL: {ex.Message}");
                                    }
                           }
                           else
                           {
                                    Log.Warning("Failed to download Oodle DLL. Some compressed assets cannot be loaded.");
                                    Log.Warning("You can manually copy oo2core_9_win64.dll to the application directory.");
                                    NotifyStatus("Compression library download failed. Continuing without it...");
                           }
                  }
                  catch (Exception ex)
                  {
                           Log.Warning($"Failed to initialize Oodle: {ex.Message}");
                  }
         }

         private void TryInitializeZlib()
         {
                  try
                  {
                           var zlibPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, ZlibHelper.DLL_NAME);

                           if (!File.Exists(zlibPath))
                           {
                                    Log.Information($"Zlib DLL not found at {zlibPath}, attempting to download...");
                                    var downloadSuccess = ZlibHelper.DownloadDll(zlibPath);

                                    if (!downloadSuccess)
                                    {
                                             Log.Warning("Failed to download Zlib DLL. Some compressed files may not be readable.");
                                             return;
                                    }
                           }

                           if (File.Exists(zlibPath))
                           {
                                    Log.Information($"Found Zlib library at: {zlibPath}");
                                    ZlibHelper.Initialize(zlibPath);
                                    Log.Information("Zlib decompression initialized successfully");
                           }
                           else
                           {
                                    Log.Warning("Zlib DLL not found. Some compressed files (.wem audio, etc.) cannot be loaded.");
                           }
                  }
                  catch (Exception ex)
                  {
                           Log.Warning($"Failed to initialize Zlib: {ex.Message}");
                  }
         }

         private static EGame ParseUeVersion(string? versionString)
         {
                  if (string.IsNullOrWhiteSpace(versionString))
                  {
                           Log.Warning("No UE version specified, defaulting to UE5_3");
                           return EGame.GAME_UE5_3;
                  }

                  var normalized = versionString.Trim();
                  for (int i = 0; i < 3 && normalized.StartsWith("UE", StringComparison.OrdinalIgnoreCase); i++)
                  {

                      if (normalized.Length <= 2) break;
                      var next = normalized[2];
                      if (char.IsDigit(next) || next == '.' || next == '_' || next == ' ' ||
                          normalized.Substring(2).StartsWith("GAME_", StringComparison.OrdinalIgnoreCase))
                      {
                          normalized = normalized.Substring(2);
                      }
                      else
                      {
                          break;
                      }
                  }

                  normalized = normalized.Trim();

                  if (normalized.StartsWith("GAME_", StringComparison.OrdinalIgnoreCase))
                  {
                        if (Enum.TryParse<EGame>(normalized, ignoreCase: true, out var parsed))
                        {
                                return parsed;
                        }
                        Log.Warning($"UE version '{versionString}' contains an unknown EGame name '{normalized}', falling back to numeric parser");
                  }

                  return normalized switch
                  {

                           "4.0" => EGame.GAME_UE4_0,
                           "4.1" => EGame.GAME_UE4_1,
                           "4.2" => EGame.GAME_UE4_2,
                           "4.3" => EGame.GAME_UE4_3,
                           "4.4" => EGame.GAME_UE4_4,
                           "4.5" => EGame.GAME_UE4_5,
                           "4.6" => EGame.GAME_UE4_6,
                           "4.7" => EGame.GAME_UE4_7,
                           "4.8" => EGame.GAME_UE4_8,
                           "4.9" => EGame.GAME_UE4_9,
                           "4.10" => EGame.GAME_UE4_10,
                           "4.11" => EGame.GAME_UE4_11,
                           "4.12" => EGame.GAME_UE4_12,
                           "4.13" => EGame.GAME_UE4_13,
                           "4.14" => EGame.GAME_UE4_14,
                           "4.15" => EGame.GAME_UE4_15,
                           "4.16" => EGame.GAME_UE4_16,
                           "4.17" => EGame.GAME_UE4_17,
                           "4.18" => EGame.GAME_UE4_18,
                           "4.19" => EGame.GAME_UE4_19,
                           "4.20" => EGame.GAME_UE4_20,
                           "4.21" => EGame.GAME_UE4_21,
                           "4.22" => EGame.GAME_UE4_22,
                           "4.23" => EGame.GAME_UE4_23,
                           "4.24" => EGame.GAME_UE4_24,
                           "4.25" => EGame.GAME_UE4_25,
                           "4.26" => EGame.GAME_UE4_26,
                           "4.27" => EGame.GAME_UE4_27,

                           "5.0" => EGame.GAME_UE5_0,
                           "5.1" => EGame.GAME_UE5_1,
                           "5.2" => EGame.GAME_UE5_2,
                           "5.3" => EGame.GAME_UE5_3,
                           "5.4" => EGame.GAME_UE5_4,
                           "5.5" => EGame.GAME_UE5_5,
                           "5.6" => EGame.GAME_UE5_6,
                           "5.7" => EGame.GAME_UE5_7,
                           "5.8" => EGame.GAME_UE5_8,
                           "5.9" => EGame.GAME_UE5_9,
                           "6.0" => EGame.GAME_UE6_0,

                           _ => ParseUeFallback(normalized)
                  };
         }

         private static EGame ParseUeFallback(string version)
         {
                  Log.Warning($"Unknown UE version '{version}', attempting to parse...");

                  if (version.StartsWith("5."))
                  {
                           Log.Information("Detected UE5.x, defaulting to UE5_3");
                           return EGame.GAME_UE5_3;
                  }
                  if (version.StartsWith("4."))
                  {
                           Log.Information("Detected UE4.x, defaulting to UE4_27");
                           return EGame.GAME_UE4_27;
                  }

                  Log.Warning("Could not parse UE version, defaulting to UE5_3");
                  return EGame.GAME_UE5_3;
         }

         public List<string> DiscoverModsInSubdirectories(string basePath)
         {
                  var mods = new List<string>();
                  var modFolderNames = new[] { "mods", "~mods", "LogicMods" };
                  var foundAnyModsDir = false;

                  try
                  {
                           foreach (var folderName in modFolderNames)
                           {
                                    var modsDir = Path.Combine(basePath, folderName);
                                    if (!Directory.Exists(modsDir))
                                             continue;

                                    foundAnyModsDir = true;
                                    Log.Information($"Scanning mods directory: {modsDir}");

                                    var utocFiles = Directory.GetFiles(modsDir, "*.utoc", SearchOption.AllDirectories);
                                    foreach (var utoc in utocFiles)
                                    {

                                             if (!utoc.EndsWith("-x", StringComparison.OrdinalIgnoreCase))
                                             {
                                                      mods.Add(utoc);
                                             }
                                    }

                                    var pakFiles = Directory.GetFiles(modsDir, "*.pak", SearchOption.AllDirectories);
                                    foreach (var pak in pakFiles)
                                    {

                                             if (pak.EndsWith("-x", StringComparison.OrdinalIgnoreCase))
                                                      continue;

                                             var baseName = Path.GetFileNameWithoutExtension(pak);
                                             var dir = Path.GetDirectoryName(pak) ?? "";
                                             var utocPath = Path.Combine(dir, baseName + ".utoc");

                                             if (!File.Exists(utocPath))
                                             {
                                                      mods.Add(pak);
                                             }
                                    }
                           }

                           if (foundAnyModsDir)
                           {
                                    Log.Information($"Discovered {mods.Count} total mods");
                           }
                           else
                           {
                                    Log.Information($"No mods directory found (checked: {string.Join(", ", modFolderNames)}) in {basePath}");
                           }
                  }
                  catch (Exception ex)
                  {
                           Log.Error(ex, "Failed to discover mods in subdirectories");
                  }

                  return mods;
         }

         public bool RegisterAndMountMod(string modPath)
         {
                  if (_provider == null)
                  {
                           Log.Warning("Provider not initialized, cannot mount mod");
                           return false;
                  }

                  try
                  {
                           var modDir = Path.GetDirectoryName(modPath) ?? "";
                           var modBaseName = Path.GetFileNameWithoutExtension(modPath);

                           modBaseName = modBaseName.Replace(".pak", "").Replace(".utoc", "").Replace(".ucas", "");

                           var alreadyMounted = _provider.MountedVfs.Any(vfs =>
                               vfs.Name.Contains(modBaseName, StringComparison.OrdinalIgnoreCase));

                           if (alreadyMounted)
                           {
                                    Log.Information($"Mod already mounted: {modBaseName}");
                                    return true;
                           }

                           var modPak = Path.Combine(modDir, $"{modBaseName}.pak");
                           var modUtoc = Path.Combine(modDir, $"{modBaseName}.utoc");
                           var modUcas = Path.Combine(modDir, $"{modBaseName}.ucas");

                           int registered = 0;

                           if (File.Exists(modPak))
                           {
                                    _provider.RegisterVfs(modPak);
                                    registered++;
                                    Log.Information($"Registered: {Path.GetFileName(modPak)}");
                           }

                           if (File.Exists(modUtoc) && File.Exists(modUcas))
                           {
                                    _provider.RegisterVfs(modUcas);
                                    _provider.RegisterVfs(modUtoc);
                                    registered += 2;
                                    Log.Information($"Registered: {Path.GetFileName(modUtoc)} + {Path.GetFileName(modUcas)}");
                           }

                           if (registered > 0)
                           {

                                    var mounted = _provider.Mount();
                                    Log.Information($"Mounted {mounted} mod archives for {modBaseName}");
                                    return mounted > 0;
                           }

                           Log.Warning($"No mod files found for {modBaseName}");
                           return false;
                  }
                  catch (Exception ex)
                  {
                           Log.Error(ex, $"Failed to register/mount mod: {modPath}");
                           return false;
                  }
         }

}
