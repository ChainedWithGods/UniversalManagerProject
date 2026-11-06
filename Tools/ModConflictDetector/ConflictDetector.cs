using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Versions;
using CUE4Parse.Encryption.Aes;
using CUE4Parse.UE4.Objects.Core.Misc;
using CUE4Parse.UE4.IO;
using CUE4Parse.UE4.Pak;

namespace ModConflictDetector;

public class ConflictDetectorOptions
{
    public required string ModsPath { get; set; }
    public string? GamePath { get; set; }
    public string? UsmapPath { get; set; }
    public string? AesKey { get; set; }
    public string UeVersion { get; set; } = "5.3";
    public bool IncludeDisabledMods { get; set; }
}

public class ConflictResult
{
    public DateTime ScanTime { get; set; } = DateTime.Now;
    public string ModsPath { get; set; } = "";
    public int TotalModsScanned { get; set; }
    public int TotalFilesScanned { get; set; }
    public int TotalConflicts { get; set; }
    public TimeSpan ScanDuration { get; set; }
    public long TotalModsSizeBytes { get; set; }
    public List<ModInfo> Mods { get; set; } = [];
    public List<FileConflict> Conflicts { get; set; } = [];
    public List<CharacterOverlap> CharacterOverlaps { get; set; } = [];
    public List<string> Errors { get; set; } = [];
    public List<string> DebugLogs { get; set; } = [];
    public List<ResolutionSuggestion> Suggestions { get; set; } = [];
    public ScanStatistics Statistics { get; set; } = new();
}

public class CharacterOverlap
{
    public string Character { get; set; } = "";
    public List<string> Mods { get; set; } = [];
    public string Severity { get; set; } = "low";
    public string Description { get; set; } = "";
    public List<string> CostumeSlots { get; set; } = [];
}

public class ScanStatistics
{
    public int IoStoreMods { get; set; }
    public int PakMods { get; set; }
    public int FolderMods { get; set; }
    public int DisabledMods { get; set; }
    public Dictionary<string, int> FileTypeCount { get; set; } = new();
    public int HighSeverityConflicts { get; set; }
    public int MediumSeverityConflicts { get; set; }
    public int LowSeverityConflicts { get; set; }
}

public class ResolutionSuggestion
{
    public string Type { get; set; } = "";
    public List<string> ModsToDisable { get; set; } = [];
    public List<string> ModsToKeep { get; set; } = [];
    public string Reason { get; set; } = "";
    public int Priority { get; set; }
    public List<string> AffectedFiles { get; set; } = [];
}

public class ModInfo
{
    public string Name { get; set; } = "";
    public string Path { get; set; } = "";
    public string Type { get; set; } = "";
    public int FileCount { get; set; }
    public bool IsDisabled { get; set; }
    public List<string> Files { get; set; } = [];
    public long SizeBytes { get; set; }
    public DateTime? LastModified { get; set; }
    public string CostumeSlot { get; set; } = "";
}

public class FileConflict
{
    public string FilePath { get; set; } = "";
    public string FileType { get; set; } = "";
    public List<string> ConflictingMods { get; set; } = [];
    public string Severity { get; set; } = "";
    public string Description { get; set; } = "";
}

public class ConflictDetector
{
    private readonly bool _verbose;
    private ConflictResult? _currentResult;
    private int _currentModIndex;
    private int _totalMods;
    private readonly System.Diagnostics.Stopwatch _stopwatch = new();
    private readonly Dictionary<string, string> _resolvedFileTypes = new(StringComparer.OrdinalIgnoreCase);

    public ConflictDetector(bool verbose = false)
    {
        _verbose = verbose;
    }

    public async Task<ConflictResult> DetectConflictsAsync(ConflictDetectorOptions options)
    {
        _stopwatch.Restart();
        var result = new ConflictResult
        {
            ModsPath = options.ModsPath
        };
        _currentResult = result;

        try
        {
            Log($"Starting conflict detection in: {options.ModsPath}");

            var modFiles = DiscoverModFiles(options.ModsPath, options.IncludeDisabledMods);
            _totalMods = modFiles.Count;
            Log($"Found {modFiles.Count} mod files");

            if (modFiles.Count == 0)
            {
                result.Errors.Add("No mod files found in the specified directory");
                return result;
            }

            var ueVersion = ParseUeVersion(options.UeVersion);
            Log($"Using UE version: {options.UeVersion} ({ueVersion})");

            var fileToMods = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);

            _currentModIndex = 0;
            foreach (var modFile in modFiles)
            {
                _currentModIndex++;
                var progress = (_currentModIndex * 100) / _totalMods;

                try
                {
                    var modInfo = await ProcessModSimpleAsync(modFile, options, ueVersion, fileToMods);
                    if (modInfo != null)
                    {
                        result.Mods.Add(modInfo);
                        result.TotalFilesScanned += modInfo.FileCount;
                        result.TotalModsSizeBytes += modInfo.SizeBytes;

                        UpdateStatistics(result.Statistics, modInfo);

                        Log($"Progress: {progress}% ({_currentModIndex}/{_totalMods}) - {modInfo.Name}: {modInfo.FileCount} files");
                    }
                }
                catch (Exception ex)
                {
                    result.Errors.Add($"Error processing {Path.GetFileName(modFile)}: {ex.Message}");
                    Log($"Error processing {modFile}: {ex}");
                }
            }

            result.TotalModsScanned = result.Mods.Count;

            foreach (var kvp in fileToMods.Where(x => x.Value.Distinct(StringComparer.OrdinalIgnoreCase).Count() > 1))
            {
                var conflictingMods = kvp.Value.Distinct(StringComparer.OrdinalIgnoreCase).ToList();
                var conflict = CreateConflict(kvp.Key, conflictingMods);
                result.Conflicts.Add(conflict);

                switch (conflict.Severity)
                {
                    case "high":
                        result.Statistics.HighSeverityConflicts++;
                        break;
                    case "medium":
                        result.Statistics.MediumSeverityConflicts++;
                        break;
                    case "low":
                        result.Statistics.LowSeverityConflicts++;
                        break;
                }
            }

            result.TotalConflicts = result.Conflicts.Count;

            result.Conflicts = result.Conflicts
                .OrderByDescending(c => c.Severity == "high" ? 3 : c.Severity == "medium" ? 2 : 1)
                .ThenBy(c => c.FilePath)
                .ToList();

            result.CharacterOverlaps = DetectCharacterOverlaps(result.Mods);

            result.Suggestions = GenerateResolutionSuggestions(result.Conflicts, result.Mods, result.CharacterOverlaps);

            _stopwatch.Stop();
            result.ScanDuration = _stopwatch.Elapsed;

            Log($"Detection complete in {result.ScanDuration.TotalSeconds:F1}s: {result.TotalConflicts} conflicts found, {result.CharacterOverlaps.Count} character overlaps");
            Log($"Summary: {result.Statistics.HighSeverityConflicts} high, {result.Statistics.MediumSeverityConflicts} medium, {result.Statistics.LowSeverityConflicts} low severity conflicts");
        }
        catch (Exception ex)
        {
            result.Errors.Add($"Fatal error: {ex.Message}");
            Log($"Fatal error: {ex}");
        }

        return result;
    }

    private List<string> DiscoverModFiles(string modsPath, bool includeDisabled)
    {
        var mods = new List<string>();

        if (!Directory.Exists(modsPath))
        {
            throw new DirectoryNotFoundException($"Mods directory not found: {modsPath}");
        }

        static bool HasEnabledCounterpart(string path) =>
            path.EndsWith("-x", StringComparison.OrdinalIgnoreCase) && File.Exists(path[..^2]);

        var utocFiles = Directory.GetFiles(modsPath, "*.utoc", SearchOption.AllDirectories);
        foreach (var utoc in utocFiles)
        {
            bool isDisabled = utoc.EndsWith(".utoc-x", StringComparison.OrdinalIgnoreCase);
            if (isDisabled && HasEnabledCounterpart(utoc))
                continue;

            if (!isDisabled || includeDisabled)
            {
                mods.Add(utoc);
            }
        }

        var utocDisabledFiles = Directory.GetFiles(modsPath, "*.utoc-x", SearchOption.AllDirectories);
        foreach (var utoc in utocDisabledFiles)
        {
            if (includeDisabled && !mods.Contains(utoc) && !HasEnabledCounterpart(utoc))
            {
                mods.Add(utoc);
            }
        }

        var pakFiles = Directory.GetFiles(modsPath, "*.pak", SearchOption.AllDirectories);
        foreach (var pak in pakFiles)
        {
            bool isDisabled = pak.EndsWith(".pak-x", StringComparison.OrdinalIgnoreCase);
            if (isDisabled && HasEnabledCounterpart(pak))
                continue;
            if (isDisabled && !includeDisabled)
                continue;

            var baseName = GetModBaseName(pak);
            var dir = Path.GetDirectoryName(pak) ?? "";
            var utocPath = Path.Combine(dir, baseName + ".utoc");
            var utocPathDisabled = Path.Combine(dir, baseName + ".utoc-x");

            if (!File.Exists(utocPath) && !File.Exists(utocPathDisabled))
            {
                mods.Add(pak);
            }
        }

        var pakDisabledFiles = Directory.GetFiles(modsPath, "*.pak-x", SearchOption.AllDirectories);
        foreach (var pak in pakDisabledFiles)
        {
            if (includeDisabled && !mods.Contains(pak) && !HasEnabledCounterpart(pak))
            {
                var baseName = GetModBaseName(pak);
                var dir = Path.GetDirectoryName(pak) ?? "";
                var utocPath = Path.Combine(dir, baseName + ".utoc");
                var utocPathDisabled = Path.Combine(dir, baseName + ".utoc-x");

                if (!File.Exists(utocPath) && !File.Exists(utocPathDisabled))
                {
                    mods.Add(pak);
                }
            }
        }

        var topLevelDirs = Directory.GetDirectories(modsPath);
        foreach (var dir in topLevelDirs)
        {
            var dirName = Path.GetFileName(dir);
            if (string.IsNullOrWhiteSpace(dirName))
                continue;

            var isDisabled = dirName.EndsWith("-x", StringComparison.OrdinalIgnoreCase);
            if (isDisabled && !includeDisabled)
                continue;

            var hasArchiveFiles =
                Directory.EnumerateFiles(dir, "*.utoc", SearchOption.AllDirectories).Any() ||
                Directory.EnumerateFiles(dir, "*.utoc-x", SearchOption.AllDirectories).Any() ||
                Directory.EnumerateFiles(dir, "*.pak", SearchOption.AllDirectories).Any() ||
                Directory.EnumerateFiles(dir, "*.pak-x", SearchOption.AllDirectories).Any();

            if (hasArchiveFiles)
                continue;

            var hasLooseFiles = Directory.EnumerateFiles(dir, "*", SearchOption.AllDirectories)
                .Any(IsLooseModFile);

            if (hasLooseFiles)
            {
                mods.Add(dir);
            }
        }

        return mods;
    }

    private async Task<ModInfo?> ProcessModSimpleAsync(
        string modPath,
        ConflictDetectorOptions options,
        EGame ueVersion,
        Dictionary<string, List<string>> fileToMods)
    {
        var isDirectory = Directory.Exists(modPath);
        var modBaseName = GetModNameFromPath(modPath);
        var isDisabled = isDirectory
            ? (Path.GetFileName(modPath.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar))?
                .EndsWith("-x", StringComparison.OrdinalIgnoreCase) ?? false)
            : modPath.EndsWith("-x", StringComparison.OrdinalIgnoreCase);
        var isIoStore = !isDirectory && (modPath.EndsWith(".utoc", StringComparison.OrdinalIgnoreCase) ||
                        modPath.EndsWith(".utoc-x", StringComparison.OrdinalIgnoreCase));

        var modType = isDirectory ? "folder" : (isIoStore ? "iostore" : "pak");

        Log($"Processing: {modBaseName} ({modType})");

        var modInfo = new ModInfo
        {
            Name = modBaseName,
            Path = modPath,
            Type = modType,
            IsDisabled = isDisabled,
            CostumeSlot = ExtractCostumeSlot(modBaseName)
        };

        if (isDirectory)
        {
            var dirInfo = new DirectoryInfo(modPath);
            modInfo.LastModified = dirInfo.LastWriteTime;
            modInfo.SizeBytes = GetDirectorySize(modPath);
        }
        else
        {
            var fileInfo = new FileInfo(isDisabled ? GetEnabledPath(modPath) : modPath);
            if (!fileInfo.Exists)
            {
                fileInfo = new FileInfo(modPath);
            }
            modInfo.LastModified = fileInfo.Exists ? fileInfo.LastWriteTime : null;
            modInfo.SizeBytes = GetModFilesSize(modPath, modBaseName, isIoStore);
        }

        if (isDirectory)
        {
            var files = GetLooseFolderFiles(modPath);
            modInfo.Files = files;
            modInfo.FileCount = files.Count;

            foreach (var file in files)
            {
                if (!fileToMods.ContainsKey(file))
                {
                    fileToMods[file] = [];
                }
                fileToMods[file].Add(modBaseName);
            }

            Log($"  Found {files.Count} files in {modBaseName} ({FormatSize(modInfo.SizeBytes)})");
            return modInfo;
        }

        var renamedFiles = new List<(string original, string renamed)>();

        try
        {
            if (isDisabled)
            {
                renamedFiles = TemporarilyEnableMod(modPath);
            }

            var actualModPath = isDisabled ? GetEnabledPath(modPath) : modPath;

            var tempDir = Path.Combine(Path.GetTempPath(), "ModConflictDetector", Guid.NewGuid().ToString());
            Directory.CreateDirectory(tempDir);

            try
            {

                CopyModFilesToTemp(actualModPath, modBaseName, tempDir, isIoStore);

                using var provider = new DefaultFileProvider(tempDir, SearchOption.TopDirectoryOnly,
                    new VersionContainer(ueVersion), StringComparer.OrdinalIgnoreCase);

                if (!string.IsNullOrWhiteSpace(options.UsmapPath) && File.Exists(options.UsmapPath))
                {
                    try
                    {
                        provider.MappingsContainer = new FileUsmapTypeMappingsProvider(options.UsmapPath);
                    }
                    catch (Exception usmapEx)
                    {
                        Log($"  Warning: Failed to load usmap (non-fatal): {usmapEx.Message}");
                    }
                }

                provider.Initialize();

                if (!string.IsNullOrWhiteSpace(options.AesKey))
                {
                    var key = new FAesKey(options.AesKey);
                    provider.SubmitKey(new FGuid(), key);
                }

                provider.Mount();

                Log($"  Mounted VFS count: {provider.MountedVfs.Count}");

                var files = new List<string>();
                var resolvedFileTypes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

                foreach (var file in provider.Files)
                {
                    var filePath = file.Key;
                    var normalizedPath = filePath.Replace('\\', '/');
                    if (!ShouldSkipFile(normalizedPath))
                    {
                        files.Add(normalizedPath);

                        if (normalizedPath.EndsWith(".uasset", StringComparison.OrdinalIgnoreCase))
                        {
                            var cueType = ResolveCUE4ParseExportType(provider, file.Value);
                            if (string.IsNullOrEmpty(cueType))
                            {
                                cueType = ResolveCUE4ParseExportType(provider, normalizedPath);
                            }
                            if (!string.IsNullOrEmpty(cueType))
                            {
                                resolvedFileTypes[normalizedPath] = cueType;
                            }
                        }
                    }
                }

                modInfo.Files = files;
                modInfo.FileCount = files.Count;

                foreach (var file in files)
                {
                    if (!fileToMods.ContainsKey(file))
                    {
                        fileToMods[file] = [];
                    }
                    fileToMods[file].Add(modBaseName);

                    if (resolvedFileTypes.TryGetValue(file, out var cueType) && !string.IsNullOrEmpty(cueType))
                    {
                        _resolvedFileTypes[file] = cueType;
                    }
                }

                Log($"  Found {files.Count} files in {modBaseName} ({FormatSize(modInfo.SizeBytes)})");
            }
            finally
            {

                CleanupTempDirectory(tempDir);
            }
        }
        catch (Exception ex)
        {
            Log($"  Error: {ex.Message}");
            throw;
        }
        finally
        {

            RestoreDisabledFiles(renamedFiles);
        }

        return modInfo;
    }

    private void CopyModFilesToTemp(string modPath, string modBaseName, string tempDir, bool isIoStore)
    {
        var modDir = Path.GetDirectoryName(modPath) ?? "";

        if (isIoStore)
        {

            var extensions = new[] { ".utoc", ".ucas", ".pak" };
            foreach (var ext in extensions)
            {
                var sourceFile = Path.Combine(modDir, modBaseName + ext);
                if (File.Exists(sourceFile))
                {
                    var destFile = Path.Combine(tempDir, modBaseName + ext);
                    File.Copy(sourceFile, destFile);
                    Log($"  Copied: {Path.GetFileName(sourceFile)}");
                }
            }
        }
        else
        {

            var sourceFile = Path.Combine(modDir, modBaseName + ".pak");
            if (File.Exists(sourceFile))
            {
                var destFile = Path.Combine(tempDir, modBaseName + ".pak");
                File.Copy(sourceFile, destFile);
                Log($"  Copied: {Path.GetFileName(sourceFile)}");
            }
        }
    }

    private bool ShouldSkipFile(string path)
    {

        return path.EndsWith(".uexp", StringComparison.OrdinalIgnoreCase) ||
               path.EndsWith(".ubulk", StringComparison.OrdinalIgnoreCase) ||
               path.EndsWith(".uptnl", StringComparison.OrdinalIgnoreCase) ||
               path.EndsWith(".ushaderbytecode", StringComparison.OrdinalIgnoreCase) ||
               path.EndsWith(".ushadercode", StringComparison.OrdinalIgnoreCase) ||
               path.EndsWith(".upipelinecache", StringComparison.OrdinalIgnoreCase) ||
               path.EndsWith(".uproject", StringComparison.OrdinalIgnoreCase) ||
               path.Contains("AssetRegistry", StringComparison.OrdinalIgnoreCase);
    }

    private bool IsLooseModFile(string filePath)
    {
        var ext = Path.GetExtension(filePath);
        return ext.Equals(".uasset", StringComparison.OrdinalIgnoreCase) ||
               ext.Equals(".umap", StringComparison.OrdinalIgnoreCase) ||
               ext.Equals(".ini", StringComparison.OrdinalIgnoreCase) ||
               ext.Equals(".locres", StringComparison.OrdinalIgnoreCase);
    }

    private List<string> GetLooseFolderFiles(string modPath)
    {
        var files = new List<string>();
        foreach (var file in Directory.EnumerateFiles(modPath, "*", SearchOption.AllDirectories))
        {
            if (!IsLooseModFile(file))
                continue;

            var relativePath = Path.GetRelativePath(modPath, file);
            var normalized = relativePath.Replace(Path.DirectorySeparatorChar, '/')
                                         .Replace(Path.AltDirectorySeparatorChar, '/');

            if (ShouldSkipFile(normalized))
                continue;

            files.Add(normalized);
        }

        return files;
    }

    private FileConflict CreateConflict(string filePath, List<string> mods)
    {

        string fileType;
        if (_resolvedFileTypes.TryGetValue(filePath, out var cueType) && !string.IsNullOrEmpty(cueType))
            fileType = cueType;
        else
            fileType = GetFileTypeFallback(filePath);

        var severity = DetermineSeverity(fileType);

        return new FileConflict
        {
            FilePath = filePath,
            FileType = fileType,
            ConflictingMods = mods,
            Severity = severity,
            Description = GetConflictDescription(fileType, severity, mods.Count)
        };
    }

    private static string? ResolveCUE4ParseExportType(CUE4Parse.FileProvider.IFileProvider provider, CUE4Parse.FileProvider.Objects.GameFile file)
    {
        try
        {
            if (!provider.TryLoadPackage(file, out var package))
                return null;

            var exports = package.GetExports();
            foreach (var export in exports)
            {
                var name = export.Name;
                if (name.StartsWith("Default__", StringComparison.Ordinal)) continue;

                var className = export.ExportType;
                if (string.IsNullOrEmpty(className)) continue;

                return MapCUE4ParseClassName(className);
            }
        }
        catch
        {

        }
        return null;
    }

    private static string? ResolveCUE4ParseExportType(CUE4Parse.FileProvider.IFileProvider provider, string normalizedPath)
    {
        try
        {
            if (!provider.TryLoadPackage(normalizedPath, out var package))
                return null;

            var exports = package.GetExports();

            foreach (var export in exports)
            {
                var name = export.Name;
                if (name.StartsWith("Default__", StringComparison.Ordinal)) continue;

                var className = export.ExportType;
                if (string.IsNullOrEmpty(className)) continue;

                return MapCUE4ParseClassName(className);
            }
        }
        catch
        {

        }
        return null;
    }

    private static string MapCUE4ParseClassName(string className)
    {
        var c = className;

        if (c.Equals("SkeletalMesh", StringComparison.OrdinalIgnoreCase) ||
            c.Equals("USkeletalMesh", StringComparison.OrdinalIgnoreCase))
            return "SkeletalMesh";

        if (c.Equals("StaticMesh", StringComparison.OrdinalIgnoreCase) ||
            c.Equals("UStaticMesh", StringComparison.OrdinalIgnoreCase))
            return "StaticMesh";

        if (c.Equals("PhysicsAsset", StringComparison.OrdinalIgnoreCase) ||
            c.Equals("UPhysicsAsset", StringComparison.OrdinalIgnoreCase))
            return "PhysicsAsset";

        if (c.Equals("Skeleton", StringComparison.OrdinalIgnoreCase) ||
            c.Equals("USkeleton", StringComparison.OrdinalIgnoreCase))
            return "Skeleton";

        if (c.Contains("AnimSequence", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("AnimMontage", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("AnimBlueprint", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("AnimComposite", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("BlendSpace", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("AimOffset", StringComparison.OrdinalIgnoreCase) ||
            c.Equals("AnimationAsset", StringComparison.OrdinalIgnoreCase))
            return "Animation";

        if (c.Contains("MaterialInstance", StringComparison.OrdinalIgnoreCase) ||
            c.Equals("Material", StringComparison.OrdinalIgnoreCase) ||
            c.Equals("UMaterial", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("MaterialFunction", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("MaterialParameterCollection", StringComparison.OrdinalIgnoreCase))
            return "Material";

        if (c.Contains("Texture", StringComparison.OrdinalIgnoreCase))
            return "Texture";

        if (c.Contains("WidgetBlueprint", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("UserWidget", StringComparison.OrdinalIgnoreCase))
            return "UIWidget";

        if (c.Contains("Blueprint", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("BlueprintGeneratedClass", StringComparison.OrdinalIgnoreCase))
            return "Blueprint";

        if (c.Equals("DataTable", StringComparison.OrdinalIgnoreCase) ||
            c.Equals("UDataTable", StringComparison.OrdinalIgnoreCase) ||
            c.Equals("CurveTable", StringComparison.OrdinalIgnoreCase) ||
            c.Equals("StringTable", StringComparison.OrdinalIgnoreCase))
            return "DataTable";

        if (c.Contains("DataAsset", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("PrimaryDataAsset", StringComparison.OrdinalIgnoreCase))
            return "DataAsset";

        if (c.Contains("Niagara", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("ParticleSystem", StringComparison.OrdinalIgnoreCase))
            return "ParticleVFX";

        if (c.Contains("Sound", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("Audio", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("SoundBank", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("AkAudio", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("Wwise", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("CriWare", StringComparison.OrdinalIgnoreCase) ||
            c.Contains("Atom", StringComparison.OrdinalIgnoreCase))
            return "Audio";

        if (c.Contains("Font", StringComparison.OrdinalIgnoreCase))
            return "Font";

        if (c.Contains("LevelSequence", StringComparison.OrdinalIgnoreCase) ||
            c.Equals("World", StringComparison.OrdinalIgnoreCase))
            return "Map";

        return c;
    }

    private static string GetFileTypeFallback(string path)
    {
        var fileName = Path.GetFileName(path);
        var ext = Path.GetExtension(path).ToLowerInvariant();

        if (ext == ".uasset")
        {

            if (fileName.StartsWith("SK_", StringComparison.OrdinalIgnoreCase) ||
                fileName.StartsWith("SKM_", StringComparison.OrdinalIgnoreCase) ||
                fileName.Contains("SkeletalMesh", StringComparison.OrdinalIgnoreCase))
                return "SkeletalMesh";

            if (fileName.StartsWith("SM_", StringComparison.OrdinalIgnoreCase) ||
                fileName.Contains("StaticMesh", StringComparison.OrdinalIgnoreCase))
                return "StaticMesh";

            if (fileName.StartsWith("MI_", StringComparison.OrdinalIgnoreCase) ||
                fileName.StartsWith("M_", StringComparison.OrdinalIgnoreCase) ||
                fileName.Contains("Material", StringComparison.OrdinalIgnoreCase))
                return "Material";

            if (fileName.StartsWith("T_", StringComparison.OrdinalIgnoreCase) ||
                fileName.StartsWith("TEX_", StringComparison.OrdinalIgnoreCase) ||
                fileName.Contains("Texture", StringComparison.OrdinalIgnoreCase))
                return "Texture";

            if (fileName.StartsWith("PHYS_", StringComparison.OrdinalIgnoreCase) ||
                fileName.Contains("PhysicsAsset", StringComparison.OrdinalIgnoreCase))
                return "PhysicsAsset";

            if (fileName.StartsWith("ABP_", StringComparison.OrdinalIgnoreCase) ||
                fileName.StartsWith("AM_", StringComparison.OrdinalIgnoreCase) ||
                fileName.Contains("_Anim", StringComparison.OrdinalIgnoreCase) ||
                fileName.Contains("Montage", StringComparison.OrdinalIgnoreCase))
                return "Animation";

            if (fileName.StartsWith("S_", StringComparison.OrdinalIgnoreCase) ||
                fileName.EndsWith("Skeleton.uasset", StringComparison.OrdinalIgnoreCase))
                return "Skeleton";

            if (fileName.StartsWith("WBP_", StringComparison.OrdinalIgnoreCase) ||
                fileName.StartsWith("UI_", StringComparison.OrdinalIgnoreCase))
                return "UIWidget";

            if (fileName.StartsWith("BP_", StringComparison.OrdinalIgnoreCase) ||
                fileName.Contains("Blueprint", StringComparison.OrdinalIgnoreCase))
                return "Blueprint";

            if (fileName.StartsWith("DA_", StringComparison.OrdinalIgnoreCase) ||
                fileName.Contains("DataAsset", StringComparison.OrdinalIgnoreCase))
                return "DataAsset";

            if (fileName.StartsWith("DT_", StringComparison.OrdinalIgnoreCase) ||
                fileName.Contains("DataTable", StringComparison.OrdinalIgnoreCase))
                return "DataTable";

            if (fileName.StartsWith("NS_", StringComparison.OrdinalIgnoreCase) ||
                fileName.StartsWith("PS_", StringComparison.OrdinalIgnoreCase) ||
                fileName.Contains("Niagara", StringComparison.OrdinalIgnoreCase))
                return "ParticleVFX";

            if (fileName.Contains("Sound", StringComparison.OrdinalIgnoreCase) ||
                fileName.Contains("Audio", StringComparison.OrdinalIgnoreCase) ||
                fileName.Contains("Wwise", StringComparison.OrdinalIgnoreCase))
                return "Audio";

            return "Asset";
        }

        return ext switch
        {
            ".umap"   => "Map",
            ".locres" => "Localization",
            ".ini"    => "Config",
            ".png" or ".jpg" or ".dds" or ".tga" => "Texture",
            ".json"   => "DataAsset",
            _         => "Other",
        };
    }

    private static string DetermineSeverity(string fileType)
    {

        if (fileType is "SkeletalMesh" or "StaticMesh" or "PhysicsAsset" or "Skeleton"
                     or "Animation" or "Blueprint" or "UIWidget" or "DataTable" or "Map")
            return "high";

        if (fileType is "Material" or "Texture" or "ParticleVFX" or "Audio"
                     or "DataAsset" or "Font" or "LevelSequence")
            return "medium";

        return "low";
    }

    private string GetConflictDescription(string fileType, string severity, int modCount)
    {
        var severityDesc = severity switch
        {
            "high" => "This conflict will likely cause crashes or broken functionality.",
            "medium" => "This conflict may cause visual glitches or unexpected behavior.",
            "low" => "This conflict is usually safe but may cause minor issues.",
            _ => "Unknown impact."
        };

        return $"{fileType} modified by {modCount} mods. {severityDesc}";
    }

    private string GetModBaseName(string path)
    {
        var name = Path.GetFileNameWithoutExtension(path);

        if (name.EndsWith("-x", StringComparison.OrdinalIgnoreCase))
            name = name[..^2];

        if (name.EndsWith(".utoc", StringComparison.OrdinalIgnoreCase))
            name = name[..^5];
        if (name.EndsWith(".ucas", StringComparison.OrdinalIgnoreCase))
            name = name[..^5];
        if (name.EndsWith(".pak", StringComparison.OrdinalIgnoreCase))
            name = name[..^4];

        return name;
    }

    private string GetModNameFromPath(string path)
    {
        if (Directory.Exists(path))
        {
            var name = Path.GetFileName(path.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)) ?? string.Empty;
            if (name.EndsWith("-x", StringComparison.OrdinalIgnoreCase))
            {
                name = name[..^2];
            }
            return name;
        }

        return GetModBaseName(path);
    }

    private string GetEnabledPath(string disabledPath)
    {

        if (disabledPath.EndsWith("-x", StringComparison.OrdinalIgnoreCase))
            return disabledPath[..^2];
        return disabledPath;
    }

    private List<(string original, string renamed)> TemporarilyEnableMod(string modPath)
    {
        var renamed = new List<(string original, string renamed)>();
        var modDir = Path.GetDirectoryName(modPath) ?? "";
        var baseName = GetModBaseName(modPath);

        var extensions = new[] { ".pak", ".utoc", ".ucas" };

        foreach (var ext in extensions)
        {
            var disabledFile = Path.Combine(modDir, $"{baseName}{ext}-x");
            var enabledFile = Path.Combine(modDir, $"{baseName}{ext}");

            if (File.Exists(disabledFile) && !File.Exists(enabledFile))
            {
                try
                {
                    File.Move(disabledFile, enabledFile);
                    renamed.Add((disabledFile, enabledFile));
                    Log($"  Temporarily enabled: {Path.GetFileName(enabledFile)}");
                }
                catch (Exception ex)
                {
                    Log($"  Warning: Could not enable {Path.GetFileName(disabledFile)}: {ex.Message}");
                }
            }
        }

        return renamed;
    }

    private void RestoreDisabledFiles(List<(string original, string renamed)> files)
    {
        foreach (var (original, renamed) in files)
        {
            try
            {
                if (File.Exists(renamed) && !File.Exists(original))
                {
                    File.Move(renamed, original);
                    Log($"  Restored: {Path.GetFileName(original)}");
                }
            }
            catch (Exception ex)
            {
                Log($"  Warning: Could not restore {Path.GetFileName(original)}: {ex.Message}");
            }
        }
    }

    private static EGame ParseUeVersion(string? versionString)
    {
        if (string.IsNullOrWhiteSpace(versionString))
            return EGame.GAME_UE5_3;

        var normalized = versionString.Replace("UE ", "").Replace("UE", "").Trim();

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
            "4.28" => EGame.GAME_UE4_28,
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

            _ => normalized.StartsWith("5.") ? EGame.GAME_UE5_LATEST :
                 normalized.StartsWith("4.") ? EGame.GAME_UE4_LATEST :
                 EGame.GAME_UE5_LATEST
        };
    }

    private List<CharacterOverlap> DetectCharacterOverlaps(List<ModInfo> mods)
    {
        var characterToMods = new Dictionary<string, HashSet<string>>(StringComparer.OrdinalIgnoreCase);
        var characterToSlots = new Dictionary<string, HashSet<string>>(StringComparer.OrdinalIgnoreCase);

        foreach (var mod in mods)
        {
            var characters = ExtractCharactersFromMod(mod);
            foreach (var character in characters)
            {
                if (!characterToMods.ContainsKey(character))
                {
                    characterToMods[character] = [];
                    characterToSlots[character] = [];
                }
                characterToMods[character].Add(mod.Name);
                if (!string.IsNullOrEmpty(mod.CostumeSlot))
                {
                    characterToSlots[character].Add(mod.CostumeSlot);
                }
            }
        }

        var overlaps = new List<CharacterOverlap>();
        foreach (var kvp in characterToMods.Where(x => x.Value.Count > 1))
        {
            var slots = characterToSlots.ContainsKey(kvp.Key)
                ? characterToSlots[kvp.Key].ToList()
                : [];

            var severity = slots.Count > 1 ? "medium" : "low";
            var description = slots.Count > 1
                ? $"{kvp.Value.Count} mods affect this character in different slots ({string.Join(", ", slots)}). High chance of visual conflicts."
                : $"{kvp.Value.Count} mods affect this character. They may conflict if they modify the same costume slot.";

            overlaps.Add(new CharacterOverlap
            {
                Character = kvp.Key,
                Mods = kvp.Value.ToList(),
                CostumeSlots = slots,
                Severity = severity,
                Description = description
            });
        }

        return overlaps.OrderByDescending(o => o.Mods.Count).ToList();
    }

    private HashSet<string> ExtractCharactersFromMod(ModInfo mod)
    {
        var characters = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        foreach (var file in mod.Files)
        {
            var lowerPath = file.ToLowerInvariant();

            var charMatch = ExtractCharacterFromPath(lowerPath);
            if (!string.IsNullOrEmpty(charMatch))
            {
                characters.Add(charMatch);
            }
        }

        return characters;
    }

    private string ExtractCharacterFromPath(string path)
    {

        var patterns = new[]
        {
            @"/character/([^/]+)/",
            @"/characters/([^/]+)/",
            @"/chara/([^/]+)/",
            @"/chr/([^/]+)/",
            @"/player/([^/]+)/",
            @"/players/([^/]+)/",
            @"/hero/([^/]+)/",
            @"/heroes/([^/]+)/",
            @"/npc/([^/]+)/",
            @"/npcs/([^/]+)/",
            @"/enemy/([^/]+)/",
            @"/enemies/([^/]+)/",
            @"/boss/([^/]+)/",
            @"/bosses/([^/]+)/",
            @"/creature/([^/]+)/",
            @"/creatures/([^/]+)/",
            @"/vehicle/([^/]+)/",
            @"/vehicles/([^/]+)/",
            @"/weapon/([^/]+)/",
            @"/weapons/([^/]+)/",
            @"/item/([^/]+)/",
            @"/items/([^/]+)/",
        };

        foreach (var pattern in patterns)
        {
            var match = System.Text.RegularExpressions.Regex.Match(path, pattern,
                System.Text.RegularExpressions.RegexOptions.IgnoreCase);
            if (match.Success)
            {
                var name = match.Groups[1].Value;

                if (!IsCommonFolder(name))
                {
                    return FormatEntityName(name);
                }
            }
        }

        return "";
    }

    private bool IsCommonFolder(string name)
    {
        var commonFolders = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            "common", "shared", "ui", "sound", "audio", "fx", "effects",
            "materials", "textures", "meshes", "animations", "blueprints",
            "maps", "levels", "data", "config", "localization", "paks",
            "base", "core", "default", "generic", "template", "test",
            "debug", "dev", "development", "content", "assets"
        };
        return commonFolders.Contains(name);
    }

    private string FormatEntityName(string name)
    {

        var formatted = name.Replace("_", " ").Replace("-", " ");
        return System.Globalization.CultureInfo.CurrentCulture.TextInfo.ToTitleCase(formatted.ToLowerInvariant());
    }

    private void Log(string message)
    {
        var timestamped = $"[{DateTime.Now:HH:mm:ss}] {message}";

        _currentResult?.DebugLogs.Add(timestamped);

        if (_verbose)
        {
            Console.Error.WriteLine(timestamped);
        }
    }

    private void UpdateStatistics(ScanStatistics stats, ModInfo modInfo)
    {
        switch (modInfo.Type)
        {
            case "iostore":
                stats.IoStoreMods++;
                break;
            case "pak":
                stats.PakMods++;
                break;
            case "folder":
                stats.FolderMods++;
                break;
        }

        if (modInfo.IsDisabled)
        {
            stats.DisabledMods++;
        }

        foreach (var file in modInfo.Files)
        {
            var fileType = _resolvedFileTypes.TryGetValue(file, out var cueType) && !string.IsNullOrEmpty(cueType)
                ? cueType
                : GetFileTypeFallback(file);
            if (!stats.FileTypeCount.ContainsKey(fileType))
            {
                stats.FileTypeCount[fileType] = 0;
            }
            stats.FileTypeCount[fileType]++;
        }
    }

    private long GetDirectorySize(string dirPath)
    {
        try
        {
            var dirInfo = new DirectoryInfo(dirPath);
            return dirInfo.EnumerateFiles("*", SearchOption.AllDirectories).Sum(f => f.Length);
        }
        catch
        {
            return 0;
        }
    }

    private long GetModFilesSize(string modPath, string baseName, bool isIoStore)
    {
        long totalSize = 0;
        var modDir = Path.GetDirectoryName(modPath) ?? "";

        try
        {
            if (isIoStore)
            {

                var extensions = new[] { ".utoc", ".ucas", ".pak" };
                foreach (var ext in extensions)
                {
                    var filePath = Path.Combine(modDir, baseName + ext);
                    if (!File.Exists(filePath))
                    {
                        filePath = Path.Combine(modDir, baseName + ext + "-x");
                    }
                    if (File.Exists(filePath))
                    {
                        totalSize += new FileInfo(filePath).Length;
                    }
                }
            }
            else
            {

                var filePath = Path.Combine(modDir, baseName + ".pak");
                if (!File.Exists(filePath))
                {
                    filePath = Path.Combine(modDir, baseName + ".pak-x");
                }
                if (File.Exists(filePath))
                {
                    totalSize += new FileInfo(filePath).Length;
                }
            }
        }
        catch
        {

        }

        return totalSize;
    }

    private string FormatSize(long bytes)
    {
        string[] sizes = { "B", "KB", "MB", "GB" };
        double len = bytes;
        int order = 0;
        while (len >= 1024 && order < sizes.Length - 1)
        {
            order++;
            len = len / 1024;
        }
        return $"{len:0.##} {sizes[order]}";
    }

    private string ExtractCostumeSlot(string modName)
    {

        var match = System.Text.RegularExpressions.Regex.Match(modName, @"\b([A-Z]\d{1,3})\b");
        return match.Success ? match.Groups[1].Value : "";
    }

    private void CleanupTempDirectory(string tempDir)
    {
        try
        {
            if (Directory.Exists(tempDir))
            {
                Directory.Delete(tempDir, true);
            }
        }
        catch (Exception ex)
        {
            Log($"  Warning: Could not cleanup temp directory: {ex.Message}");
        }
    }

    private List<ResolutionSuggestion> GenerateResolutionSuggestions(
        List<FileConflict> conflicts,
        List<ModInfo> mods,
        List<CharacterOverlap> characterOverlaps)
    {
        var suggestions = new List<ResolutionSuggestion>();

        var modConflictGroups = new Dictionary<string, List<FileConflict>>();

        foreach (var conflict in conflicts)
        {
            var key = string.Join("|", conflict.ConflictingMods.OrderBy(m => m));
            if (!modConflictGroups.ContainsKey(key))
            {
                modConflictGroups[key] = [];
            }
            modConflictGroups[key].Add(conflict);
        }

        foreach (var group in modConflictGroups)
        {
            var conflictingMods = group.Key.Split('|').ToList();
            var groupConflicts = group.Value;

            var disabledMods = conflictingMods
                .Where(modName => mods.FirstOrDefault(m => m.Name == modName)?.IsDisabled == true)
                .ToList();

            if (disabledMods.Count > 0)
            {
                suggestions.Add(new ResolutionSuggestion
                {
                    Type = "safe",
                    ModsToKeep = conflictingMods.Except(disabledMods).ToList(),
                    ModsToDisable = disabledMods,
                    Reason = $"One or more conflicting mods are already disabled: {string.Join(", ", disabledMods)}",
                    Priority = 1,
                    AffectedFiles = groupConflicts.Select(c => c.FilePath).ToList()
                });
                continue;
            }

            var highSeverity = groupConflicts.Where(c => c.Severity == "high").ToList();
            if (highSeverity.Count > 0)
            {
                var modInfos = conflictingMods
                    .Select(name => mods.FirstOrDefault(m => m.Name == name))
                    .Where(m => m != null)
                    .ToList();

                var oldestMod = modInfos
                    .Where(m => m!.LastModified.HasValue)
                    .OrderBy(m => m!.LastModified)
                    .FirstOrDefault();

                if (oldestMod != null)
                {
                    suggestions.Add(new ResolutionSuggestion
                    {
                        Type = "disable",
                        ModsToKeep = [oldestMod.Name],
                        ModsToDisable = conflictingMods.Except([oldestMod.Name]).ToList(),
                        Reason = $"High severity conflict detected. '{oldestMod.Name}' appears to be the established mod (older). Consider disabling the others.",
                        Priority = 10,
                        AffectedFiles = highSeverity.Select(c => c.FilePath).ToList()
                    });
                }
                else
                {
                    suggestions.Add(new ResolutionSuggestion
                    {
                        Type = "manual",
                        ModsToKeep = [],
                        ModsToDisable = conflictingMods,
                        Reason = "High severity conflict detected. Manual review recommended to determine which mod to keep.",
                        Priority = 9,
                        AffectedFiles = highSeverity.Select(c => c.FilePath).ToList()
                    });
                }
            }

            else
            {
                suggestions.Add(new ResolutionSuggestion
                {
                    Type = "manual",
                    ModsToKeep = [],
                    ModsToDisable = conflictingMods,
                    Reason = $"Multiple mods modify the same files ({groupConflicts.Count} conflicts). Review to ensure intended behavior.",
                    Priority = 5,
                    AffectedFiles = groupConflicts.Select(c => c.FilePath).ToList()
                });
            }
        }

        foreach (var overlap in characterOverlaps)
        {
            if (overlap.CostumeSlots.Count > 1)
            {
                suggestions.Add(new ResolutionSuggestion
                {
                    Type = "keep_one",
                    ModsToDisable = overlap.Mods,
                    Reason = $"Multiple mods affect character '{overlap.Character}' in different costume slots: {string.Join(", ", overlap.CostumeSlots)}. Keep only one per character slot.",
                    Priority = 3,
                    AffectedFiles = []
                });
            }
        }

        return suggestions.OrderByDescending(s => s.Priority).ToList();
    }
}
