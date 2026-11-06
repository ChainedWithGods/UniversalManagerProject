using System;
using System.Collections.Generic;
using System.Dynamic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using PakViewer.ViewModels;
using Serilog;

namespace PakViewer.ViewModels;

public enum ArchiveType
{
    GameFile,
    Mod,
    Unknown
}

public class ArchiveViewModel : ViewModelBase
{
    private readonly dynamic? _vfs;
    private readonly List<object>? _members;
    private readonly string? _unmountedPath;
    private readonly string? _unmountedName;
    private readonly bool _isUnmounted;
    private readonly bool _isBaseGame;
    private ArchiveType? _cachedArchiveType;

    public ArchiveViewModel(object vfs, bool isBaseGame = false)
    {
        _vfs = vfs;
        _members = null;
        _isUnmounted = false;
        _isBaseGame = isBaseGame;
    }

    public ArchiveViewModel(object primaryVfs, List<object> members, bool isBaseGame = false)
    {
        _vfs = primaryVfs;
        _members = members;
        _isUnmounted = false;
        _isBaseGame = isBaseGame;
    }

    public ArchiveViewModel(string modPath, string modName, bool isUnmounted)
    {
        _unmountedPath = modPath;
        _unmountedName = modName;
        _isUnmounted = isUnmounted;
        _vfs = null;
        _members = null;
        _isBaseGame = false;
    }

    public bool IsUnmounted => _isUnmounted;

    public string? UnmountedPath => _unmountedPath;

    public bool IsGroup => _members != null && _members.Count > 1;

    public ArchiveType ArchiveType
    {
        get
        {
            if (_cachedArchiveType.HasValue)
                return _cachedArchiveType.Value;

            _cachedArchiveType = DetectArchiveType();
            return _cachedArchiveType.Value;
        }
    }

    public bool IsMod => ArchiveType == ArchiveType.Mod;

    public bool IsGameFile => ArchiveType == ArchiveType.GameFile;

    private ArchiveType DetectArchiveType()
    {

        if (_isUnmounted)
            return ArchiveType.Mod;

        if (_isBaseGame)
            return ArchiveType.GameFile;

        var name = Name;
        if (string.IsNullOrEmpty(name))
            return ArchiveType.Unknown;

        var gameFilePatterns = new[]
        {
            @"^pakchunk\d+",
            @"^global$",
            @"^engine$",
            @"WindowsNoEditor",
            @"WindowsClient",
            @"LinuxNoEditor",
            @"^Startup",
            @"^ShaderArchive",
        };

        foreach (var pattern in gameFilePatterns)
        {
            if (Regex.IsMatch(name, pattern, RegexOptions.IgnoreCase))
                return ArchiveType.GameFile;
        }

        var modPatterns = new[]
        {
            @"^\d+[-_]",
            @"[-_]\d{10,}_P$",
            @"_P$",
            @"[-_]v?\d+\.\d+",
            @"^[A-Z]\d+\s",
            @"Mod$",
            @"^mod[-_]",
        };

        foreach (var pattern in modPatterns)
        {
            if (Regex.IsMatch(name, pattern, RegexOptions.IgnoreCase))
                return ArchiveType.Mod;
        }

        var mountPoint = MountPoint;
        if (!string.IsNullOrEmpty(mountPoint) && mountPoint != "/" && mountPoint != "../../../")
        {

            if (!mountPoint.Contains("Engine") && !mountPoint.Contains("Game"))
                return ArchiveType.Mod;
        }

        if (Size < 100 * 1024 * 1024)
        {

            if (!name.Contains("pakchunk", StringComparison.OrdinalIgnoreCase) &&
                !name.Contains("WindowsNoEditor", StringComparison.OrdinalIgnoreCase))
            {
                return ArchiveType.Mod;
            }
        }

        return ArchiveType.GameFile;
    }

    public IReadOnlyList<string>? MemberNames
    {
        get
        {
            if (_members == null) return null;
            return _members.Select(m => GetString(m, "Name") ?? string.Empty)
                           .Where(n => !string.IsNullOrEmpty(n))
                           .ToList();
        }
    }

    public string Name
    {
        get
        {

            if (_isUnmounted && !string.IsNullOrEmpty(_unmountedName))
            {
                var baseName = Path.GetFileNameWithoutExtension(_unmountedName);
                if (baseName.EndsWith("-x", StringComparison.OrdinalIgnoreCase))
                    baseName = baseName[..^2];
                return baseName;
            }

            if (_vfs == null) return _unmountedName ?? "Unknown";

            var name = GetString(_vfs, "Name") ?? string.Empty;
            if (IsGroup)
            {
                try
                {
                    var baseName = Path.GetFileNameWithoutExtension(name);
                    if (baseName.EndsWith("-x", StringComparison.OrdinalIgnoreCase))
                        baseName = baseName[..^2];
                    return baseName;
                }
                catch { }
            }
            return name;
        }
    }

    public long Size
    {
        get
        {

            if (_isUnmounted && !string.IsNullOrEmpty(_unmountedPath))
            {
                try
                {
                    long totalSize = 0;
                    var dir = Path.GetDirectoryName(_unmountedPath) ?? "";
                    var baseName = Path.GetFileNameWithoutExtension(_unmountedPath);

                    baseName = baseName.Replace(".pak", "").Replace(".utoc", "").Replace(".ucas", "");
                    if (baseName.EndsWith("-x", StringComparison.OrdinalIgnoreCase))
                        baseName = baseName[..^2];

                    var pakPath = Path.Combine(dir, $"{baseName}.pak");
                    var utocPath = Path.Combine(dir, $"{baseName}.utoc");
                    var ucasPath = Path.Combine(dir, $"{baseName}.ucas");

                    if (File.Exists(pakPath))
                        totalSize += new FileInfo(pakPath).Length;
                    if (File.Exists(utocPath))
                        totalSize += new FileInfo(utocPath).Length;
                    if (File.Exists(ucasPath))
                        totalSize += new FileInfo(ucasPath).Length;

                    return totalSize;
                }
                catch { }
                return 0L;
            }

            if (_vfs == null) return 0L;

            if (IsGroup)
            {
                return _members!.Sum(m => GetLong(m, "Length"));
            }
            return GetLong(_vfs, "Length");
        }
    }

    public bool IsEncrypted
    {
        get
        {
            if (_isUnmounted) return false;
            if (_vfs == null) return false;

            if (IsGroup)
            {
                return _members!.Any(m => GetBool(m, "IsEncrypted"));
            }
            return GetBool(_vfs, "IsEncrypted");
        }
    }

    public string MountPoint
    {
        get
        {
            if (_isUnmounted) return "(not mounted)";
            if (_vfs == null) return string.Empty;

            try { return GetString(_vfs, "MountPoint") ?? string.Empty; }
            catch { return string.Empty; }
        }
    }

    private int? _cachedFileCount;

    public string FileCount
    {
        get
        {
            if (_isUnmounted)
            {

                if (!_cachedFileCount.HasValue)
                {
                    _cachedFileCount = TryGetUnmountedFileCount();
                }

                if (_cachedFileCount.HasValue && _cachedFileCount.Value > 0)
                {
                    return $"{_cachedFileCount.Value} files";
                }

                if (_cachedFileCount.HasValue && _cachedFileCount.Value == -1)
                {

                    return "pak";
                }

                return "—";
            }
            if (_vfs == null) return "0 files";

            if (IsGroup)
            {
                var total = _members!.Sum(m => GetInt(m, "FileCount"));
                return $"{total} files";
            }
            return $"{GetInt(_vfs, "FileCount")} files";
        }
    }

    public string FileCountDisplay => FileCount;

    private int TryGetUnmountedFileCount()
    {
        if (string.IsNullOrEmpty(_unmountedPath))
            return 0;

        try
        {
            var dir = Path.GetDirectoryName(_unmountedPath) ?? "";
            var baseName = Path.GetFileNameWithoutExtension(_unmountedPath);

            baseName = baseName.Replace(".pak", "").Replace(".utoc", "").Replace(".ucas", "");
            if (baseName.EndsWith("-x", StringComparison.OrdinalIgnoreCase))
                baseName = baseName[..^2];

            var utocPath = Path.Combine(dir, $"{baseName}.utoc");
            var utocPathDisabled = Path.Combine(dir, $"{baseName}.utoc-x");

            string? actualUtocPath = null;
            if (File.Exists(utocPath))
                actualUtocPath = utocPath;
            else if (File.Exists(utocPathDisabled))
                actualUtocPath = utocPathDisabled;

            if (actualUtocPath != null)
            {
                var count = ReadUtocFileCount(actualUtocPath);
                if (count > 0)
                    return count;
            }

            var pakPath = Path.Combine(dir, $"{baseName}.pak");
            var pakPathDisabled = Path.Combine(dir, $"{baseName}.pak-x");

            string? actualPakPath = null;
            if (File.Exists(pakPath))
                actualPakPath = pakPath;
            else if (File.Exists(pakPathDisabled))
                actualPakPath = pakPathDisabled;

            if (actualPakPath != null)
            {
                var count = ReadPakFileCount(actualPakPath);
                if (count > 0)
                    return count;
            }
        }
        catch (Exception ex)
        {
            Log.Debug($"Failed to read file count for unmounted mod: {ex.Message}");
        }

        return 0;
    }

    private static int ReadUtocFileCount(string utocPath)
    {
        try
        {
            using var fs = new FileStream(utocPath, FileMode.Open, FileAccess.Read, FileShare.Read);
            using var reader = new BinaryReader(fs);

            var magic = reader.ReadBytes(16);
            byte[] expectedMagic = { 0x2D, 0x3D, 0x3D, 0x2D, 0x2D, 0x3D, 0x3D, 0x2D, 0x2D, 0x3D, 0x3D, 0x2D, 0x2D, 0x3D, 0x3D, 0x2D };

            if (!magic.SequenceEqual(expectedMagic))
            {
                Log.Debug($"Invalid utoc magic in {utocPath}");
                return 0;
            }

            reader.ReadBytes(8);

            var tocEntryCount = reader.ReadUInt32();

            return Math.Max(0, (int)tocEntryCount - 1);
        }
        catch (Exception ex)
        {
            Log.Debug($"Error reading utoc header from {utocPath}: {ex.Message}");
            return 0;
        }
    }

    private static int ReadPakFileCount(string pakPath)
    {
        try
        {
            using var fs = new FileStream(pakPath, FileMode.Open, FileAccess.Read, FileShare.Read);

            if (fs.Length < 50)
                return -1;

            const uint PAK_FILE_MAGIC = 0x5A6F12E1;

            int[] offsets = { 45, 53, 61, 157, 189, 221 };

            foreach (var offset in offsets)
            {
                if (fs.Length < offset)
                    continue;

                fs.Seek(-offset, SeekOrigin.End);
                using var reader = new BinaryReader(fs, System.Text.Encoding.UTF8, leaveOpen: true);

                var potentialMagic = reader.ReadUInt32();
                if (potentialMagic == PAK_FILE_MAGIC)
                {

                    var version = reader.ReadInt32();

                    if (version < 1 || version > 20)
                        continue;

                    var indexOffset = reader.ReadInt64();
                    var indexSize = reader.ReadInt64();

                    if (indexOffset < 0 || indexOffset >= fs.Length || indexSize <= 0 || indexSize > fs.Length)
                        continue;

                    fs.Seek(indexOffset, SeekOrigin.Begin);

                    var mountPointLength = reader.ReadInt32();
                    if (mountPointLength < 0 || mountPointLength > 1024)
                    {

                        var estimatedCount = (int)(indexSize / 100);
                        return Math.Max(1, estimatedCount);
                    }

                    if (mountPointLength > 0)
                    {
                        reader.ReadBytes(mountPointLength);
                    }

                    var fileCount = reader.ReadInt32();
                    if (fileCount >= 0 && fileCount < 1000000)
                    {
                        return fileCount;
                    }
                }
            }

            return -1;
        }
        catch (Exception ex)
        {
            Log.Debug($"Error reading pak file count from {pakPath}: {ex.Message}");
            return -1;
        }
    }

    public string TypeBadge
    {
        get
        {
            return ArchiveType switch
            {
                ArchiveType.Mod => "MOD",
                ArchiveType.GameFile => "GAME",
                _ => ""
            };
        }
    }

    public string FormattedSize
    {
        get
        {
            double len = Size;
            string[] sizes = { "B", "KB", "MB", "GB", "TB" };
            int order = 0;
            while (len >= 1024 && order < sizes.Length - 1)
            {
                order++;
                len = len / 1024;
            }
            return $"{len:0.##} {sizes[order]}";
        }
    }

    private static string? GetString(object dyn, string propName)
    {
        try
        {
            var val = dyn.GetType().GetProperty(propName)?.GetValue(dyn);
            return val?.ToString();
        }
        catch { return null; }
    }

    private static long GetLong(object dyn, string propName)
    {
        try
        {
            var val = dyn.GetType().GetProperty(propName)?.GetValue(dyn);
            if (val == null) return 0L;
            if (val is long l) return l;
            return Convert.ToInt64(val);
        }
        catch { return 0L; }
    }

    private static int GetInt(object dyn, string propName)
    {
        try
        {
            var val = dyn.GetType().GetProperty(propName)?.GetValue(dyn);
            if (val == null) return 0;
            if (val is int i) return i;
            return Convert.ToInt32(val);
        }
        catch { return 0; }
    }

    private static bool GetBool(object dyn, string propName)
    {
        try
        {
            var val = dyn.GetType().GetProperty(propName)?.GetValue(dyn);
            if (val == null) return false;
            if (val is bool b) return b;
            return Convert.ToBoolean(val);
        }
        catch { return false; }
    }
}
