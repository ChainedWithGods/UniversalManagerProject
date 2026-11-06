using System.IO;
using System.Text;
using Microsoft.UI.Xaml.Media.Imaging;
using Windows.Storage.Streams;
using CUE4Parse.FileProvider;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Exports.Material;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse.UE4.Assets.Exports.StaticMesh;
using CUE4Parse.UE4.Assets.Exports.SkeletalMesh;
using CUE4Parse.UE4.Objects.Core.Misc;
using CUE4Parse.UE4.Objects.UObject;
using CUE4Parse_Conversion.Textures;
using CUE4Parse.UE4.AssetRegistry;
using CUE4Parse.UE4.Assets.Exports.Criware;
using CUE4Parse.UE4.Assets.Exports.Sound;
using CUE4Parse.UE4.Assets.Exports.Wwise;
using CUE4Parse.UE4.BinaryConfig;
using CUE4Parse.UE4.Criware;
using CUE4Parse.UE4.Criware.Decoders.ADX;
using CUE4Parse.UE4.Criware.Decoders.HCA;
using CUE4Parse.UE4.Criware.Readers;
using CUE4Parse.UE4.FMod;
using CUE4Parse.UE4.Localization;
using CUE4Parse.UE4.Oodle.Objects;
using CUE4Parse.UE4.Shaders;
using CUE4Parse.UE4.Wwise;
using CUE4Parse_Conversion.Sounds;
using Newtonsoft.Json;
using SkiaSharp;
using Svg.Skia;
using PakViewer.Models;
using PakViewer.Settings;
using PakViewer.Snooper;
using Serilog;

namespace PakViewer.Services;

public class FilePreviewService
{
    private readonly Dictionary<string, PreviewResult> _cache = new();
    private readonly Dictionary<string, Task<PreviewResult>> _inflight = new();
    private readonly LinkedList<string> _lruList = new();

    private CriWareProvider? _criWareProvider;
    private string? _currentGameDirectory;
    private const long DefaultPreviewSizeLimit = 50L * 1024 * 1024;
    private const int MaxCacheEntries = 30;

    public async Task<PreviewResult> LoadFileAsync(IFileProvider? provider, string filePath)
    {
        if (provider == null)
            return new PreviewResult { TextContent = "No provider loaded" };

        if (_cache.TryGetValue(filePath, out var cached))
            return cached;

        if (_inflight.TryGetValue(filePath, out var existing))
            return await existing;

        var task = LoadFileInternalAsync(provider, filePath);
        _inflight[filePath] = task;
        try
        {
            var result = await task;
            FinalizePreviewResult(result);

            while (_cache.Count >= MaxCacheEntries && _lruList.Count > 0)
            {
                var oldest = _lruList.First!.Value;
                _lruList.RemoveFirst();
                if (_cache.Remove(oldest, out var oldResult))
                {
                    oldResult.Dispose();
                }
            }

            _cache[filePath] = result;
            _lruList.AddLast(filePath);
            return result;
        }
        finally
        {
            _inflight.Remove(filePath);
        }
    }

    public bool IsCached(string filePath)
    {
        if (_cache.ContainsKey(filePath))
        {

            _lruList.Remove(filePath);
            _lruList.AddLast(filePath);
            return true;
        }
        return false;
    }

    private async Task<PreviewResult> LoadFileInternalAsync(IFileProvider? provider, string filePath)
    {
        try
        {
            if (!provider!.Files.TryGetValue(filePath, out var file))
            {
                if (provider is CUE4Parse.FileProvider.Vfs.AbstractVfsFileProvider vfs)
                {
                    vfs.Mount();
                }
                file = provider.Files[filePath];
            }
            var ext = Path.GetExtension(filePath).ToLowerInvariant();
            if (file.Size > DefaultPreviewSizeLimit && !CanPreviewLargeFile(ext))
            {
                return new PreviewResult
                {
                    IsText = true,
                    TextContent = $"File too large to preview ({FormatFileSize(file.Size)})\nUse Export to save to disk."
                };
            }

            var data = await file.ReadAsync();

            var deferredAudioResult = await TryHandleDeferredAudioPreviewAsync(ext, data, filePath);
            if (deferredAudioResult != null)
                return deferredAudioResult;

            var extResult = await HandleByExtensionAsync(ext, data, provider, filePath);
            if (extResult != null)
                return extResult;

            if (ext == ".uasset")
            {
                try
                {
                    var package = await Task.Run(() => provider.LoadPackage(filePath));
                    foreach (var exportLazy in package.ExportsLazy)
                    {
                        var obj = exportLazy?.Value;
                        if (obj is USoundAtomCueSheet cueSheet)
                        {
                            EnsureCriWareProvider(provider);
                            var lazyResult = BuildLazyCriWareCueSheetPreview(
                                cueSheet.Name,
                                cueSheet.AcbReader);
                            if (lazyResult != null)
                            {
                                return lazyResult;
                            }
                        }
                        else if (obj is UAtomCueSheet atomCueSheet)
                        {
                            EnsureCriWareProvider(provider);
                            var lazyResult = BuildLazyCriWareCueSheetPreview(
                                atomCueSheet.Name,
                                atomCueSheet.AcbReader);
                            if (lazyResult != null)
                            {
                                return lazyResult;
                            }
                        }
                        else if (obj is USoundAtomCue or UAtomWaveBank)
                        {
                            EnsureCriWareProvider(provider);
                            List<CriWareExtractedSound>? extractedSounds = null;
                            if (obj is USoundAtomCue soundAtomCue) extractedSounds = _criWareProvider?.ExtractCriWareSounds(soundAtomCue);
                            else if (obj is UAtomWaveBank atomWaveBank) extractedSounds = _criWareProvider?.ExtractCriWareSounds(atomWaveBank);

                            if (extractedSounds != null && extractedSounds.Count > 0)
                            {
                                var sounds = extractedSounds.Select(ConvertCriWareSound).ToList();
                                string formatInfo = extractedSounds[0].Extension.ToUpperInvariant();

                                return new PreviewResult
                                {
                                    IsAudio = true,
                                    AudioSounds = sounds,
                                    TextContent = $"🎵 CriWare Asset: {obj.ExportType}\nTracks: {sounds.Count}\nFormat: {formatInfo}",
                                    AssetCategory = EAssetCategory.Audio,
                                    AssetActions = EBulkType.Audio
                                };
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    Log.Warning($"Failed to load CriWare package {filePath}: {ex.Message}");
                }
            }

            if (ext == ".uasset" || ext == ".umap" || ext == ".utexture")
            {
                try
                {
                    var package = await Task.Run(() => provider.LoadPackage(filePath));
                    var name = Path.GetFileNameWithoutExtension(filePath);
                    var (category, actions) = AssetTypeDetector.Detect(package, name);

                    var packageAudioResult = TryBuildPackageAudioPreview(package, filePath);
                    if (packageAudioResult != null)
                    {
                        packageAudioResult.AssetCategory = category == EAssetCategory.All ? EAssetCategory.Audio : category;
                        packageAudioResult.AssetActions = actions == EBulkType.None ? EBulkType.Audio : actions;
                        return packageAudioResult;
                    }

                    var result = new PreviewResult
                    {
                        IsText = true,
                        AssetCategory = category,
                        AssetActions = actions
                    };

                    var json = JsonConvert.SerializeObject(package, Formatting.Indented, new JsonSerializerSettings
                    {
                        ReferenceLoopHandling = ReferenceLoopHandling.Ignore,
                        NullValueHandling = NullValueHandling.Ignore
                    });
                    result.TextContent = json;

                    if (category.IsOfCategory(EAssetCategory.Materials))
                    {
                        foreach (var exportLazy in package.ExportsLazy)
                        {
                            var obj = exportLazy?.Value;
                            if (obj is UMaterialInstanceConstant or UMaterialInstanceDynamic or UMaterial)
                            {
                                var matResult = await LoadMaterialAsync(provider, obj, data);
                                result.Image = matResult.Image;
                                result.MaterialData = matResult.MaterialData;
                                result.TextContent = matResult.TextContent;
                                return result;
                            }
                        }
                    }

                    if (category == EAssetCategory.Texture)
                    {
                        result.IsTexture = true;
                        foreach (var exportLazy in package.ExportsLazy)
                        {
                            var obj = exportLazy?.Value;
                            if (obj is UTexture2D texture)
                            {
                                var textureJson = JsonConvert.SerializeObject(texture, Formatting.Indented, new JsonSerializerSettings
                                {
                                    ReferenceLoopHandling = ReferenceLoopHandling.Ignore,
                                    NullValueHandling = NullValueHandling.Ignore
                                });
                                result.TextContent = textureJson;

                                var stream = LoadTextureStream(texture);
                                if (stream != null)
                                {

                                    var dispatcher = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();
                                    if (dispatcher != null)
                                    {
                                        var tcs = new TaskCompletionSource<BitmapImage?>();
                                        dispatcher.TryEnqueue(() =>
                                        {
                                            try
                                            {
                                                var bitmapImage = new BitmapImage();
                                                bitmapImage.SetSource(stream.AsRandomAccessStream());
                                                tcs.SetResult(bitmapImage);
                                            }
                                            catch (Exception ex)
                                            {
                                                stream.Dispose();
                                                Log.Error(ex, "Failed to create BitmapImage on UI thread");
                                                tcs.SetResult(null);
                                            }
                                        });

                                        var bitmap = await tcs.Task;
                                        if (bitmap != null)
                                        {
                                            result.Image = bitmap;
                                            return result;
                                        }
                                    }
                                    else
                                    {
                                        stream.Dispose();
                                    }
                                }

                                var mip = texture.GetFirstMip();
                                result.TextContent = $"Texture Preview Error\n\n" +
                                             $"Format: {texture.Format}\n" +
                                             $"Size: {mip?.SizeX ?? 0} x {mip?.SizeY ?? 0}\n\n" +
                                             $"This texture format could not be decoded for preview.\n" +
                                             $"You can still export it using the Export button.\n\n" +
                                             $"Full texture data:\n\n{textureJson}";
                                return result;
                            }
                        }
                    }

                    if (category == EAssetCategory.StaticMesh || category == EAssetCategory.SkeletalMesh)
                    {
                        result.IsModel = true;
                        foreach (var exportLazy in package.ExportsLazy)
                        {
                            var obj = exportLazy?.Value;
                            if (obj is UStaticMesh staticMesh)
                            {
                                result.ModelName = staticMesh.Name;
                                break;
                            }
                            else if (obj is USkeletalMesh skeletalMesh)
                            {
                                result.ModelName = skeletalMesh.Name;
                                break;
                            }
                        }
                    }

                    return result;
                }
                catch (Exception ex)
                {
                    Log.Warning($"Failed to load as package: {ex.Message}");
                }
            }

                return new PreviewResult
                {
                    IsText = true,
                    TextContent = $"Binary file ({FormatFileSize(file.Size)})\nPreview not available."
                };
        }
        catch (Exception ex)
        {
            var errorMsg = new StringBuilder();
            errorMsg.AppendLine("Error loading file:");
            errorMsg.AppendLine(ex.Message);

            if (ex.Message.Contains("Oodle") || ex.Message.Contains("decompression"))
            {
                errorMsg.AppendLine();
                errorMsg.AppendLine("This file requires Oodle decompression which is not available.");
                errorMsg.AppendLine("You can still export the file in its compressed form.");
            }
            else if (ex.ToString().Contains("Unknown property") && ex.ToString().Contains("ParserException"))
            {
                errorMsg.AppendLine();
                errorMsg.AppendLine("MAPPING ERROR");
                errorMsg.AppendLine("This file uses unversioned properties and cannot be read without a compatible .usmap mapping file.");
                errorMsg.AppendLine("Please go to Settings and select the correct .usmap file for this game.");
            }

            Log.Warning($"Failed to preview {filePath}: {ex.Message}");

            return new PreviewResult
            {
                IsText = true,
                TextContent = errorMsg.ToString()
            };
        }
    }

    public void ClearCacheFor(string filePath)
    {
        _lruList.Remove(filePath);
        if (_cache.Remove(filePath, out var result))
        {
            result.Dispose();
        }
    }

    public void ClearAllCache()
    {

        foreach (var result in _cache.Values)
        {
            result.Dispose();
        }
        _cache.Clear();
        _lruList.Clear();
        _inflight.Clear();

        GC.Collect(GC.MaxGeneration, GCCollectionMode.Optimized);
        GC.WaitForPendingFinalizers();
        GC.Collect(GC.MaxGeneration, GCCollectionMode.Optimized);
    }

    public async Task ExportFileAsync(IFileProvider? provider, string filePath, string outputPath)
    {
        if (provider == null)
            throw new InvalidOperationException("No provider loaded");

        try
        {
            var file = provider.Files[filePath];
            var data = await file.ReadAsync();
            await File.WriteAllBytesAsync(outputPath, data);
        }
        catch (Exception ex)
        {
            if (ex.Message.Contains("Oodle") || ex.Message.Contains("decompression"))
            {
                throw new Exception("Cannot export this file: Oodle decompression is required but not available. " +
                                  "The file is compressed and cannot be extracted without the Oodle library.", ex);
            }
            throw;
        }
    }

    public string GetFileProperties(IFileProvider? provider, string filePath)
    {
        if (provider == null)
            return "No provider loaded";

        try
        {
            var file = provider.Files[filePath];
            var sb = new StringBuilder();

            sb.AppendLine($"Name: {file.Name}");
            sb.AppendLine($"Path: {file.Path}");
            sb.AppendLine($"Extension: {file.Extension}");
            sb.AppendLine($"Size: {FormatFileSize(file.Size)}");

            if (file is CUE4Parse.FileProvider.Objects.GameFile gameFile)
            {
                sb.AppendLine($"Is Encrypted: {gameFile.IsEncrypted}");
            }

            return sb.ToString();
        }
        catch (Exception ex)
        {
            return $"Error getting properties: {ex.Message}";
        }
    }

    private bool IsTextFile(string ext)
    {
        return ext switch
        {

            ".txt" or ".json" or ".xml" or ".ini" or ".cfg" or ".log" or ".md" => true,

            ".uproject" or ".uefnproject" or ".uplugin" or ".upluginmanifest" or ".uparam" => true,
            ".code-workspace" or ".projectstore" or ".manifest" or ".archive" => true,

            ".csv" or ".tsv" or ".yml" or ".lua" or ".cpp" or ".h" or ".js" or ".css" => true,
            ".html" or ".bat" or ".vdf" or ".scss" or ".glslfx" or ".verse" => true,

            ".pem" or ".gitignore" or ".json5" or ".po" => true,

            ".dat" or ".data" or ".doc" or ".apx" or ".udn" or ".tpl" => true,

            ".cube" or ".usda" or ".ocio" or ".cptake" or ".vmodule" or ".spi1d" => true,
            _ => false
        };
    }

    private static bool CanPreviewLargeFile(string ext)
    {
        return ext switch
        {
            ".awb" or ".acb" or ".bnk" or ".pck" or ".bank"
            or ".wem" or ".wav" or ".ogg" or ".flac" or ".mp4"
            or ".opus" or ".mp3" or ".mid" or ".hca" or ".adx"
            or ".xvag" or ".at9" => true,
            _ => false
        };
    }

    private void EnsureCriWareProvider(IFileProvider provider)
    {
        var gameDir = AppSettings.Instance.Games.FirstOrDefault(g => g.Name == AppSettings.Instance.LastSelectedGame)?.PakPath;
        if (string.IsNullOrEmpty(gameDir)) return;

        if (_criWareProvider == null || _currentGameDirectory != gameDir)
        {
            _currentGameDirectory = gameDir;
            _criWareProvider = new CriWareProvider(provider, gameDir);
        }
    }

    private async Task<PreviewResult?> TryHandleDeferredAudioPreviewAsync(string ext, byte[] data, string filePath)
    {
        return ext switch
        {
            ".awb" => await Task.Run(() => BuildAwbPreviewResult(data, filePath)),
            _ => null
        };
    }

    private PreviewResult BuildAwbPreviewResult(byte[] data, string filePath)
    {
        using var stream = new MemoryStream(data) { Position = 0 };
        using var awbReader = new AwbReader(stream);

        var tracks = awbReader.Waves
            .Select(wave => new AudioTrackInfo
            {
                Name = $"Wave_{wave.WaveId}",
                Format = "hca",
                OriginalFormat = "hca",
                DataLoader = CreateAwbTrackLoader(data, (ushort)wave.WaveId)
            })
            .ToList();

        var previewLines = tracks
            .Take(20)
            .Select(track => track.Name)
            .ToList();

        var summary = new StringBuilder();
        summary.AppendLine($"CriWare AWB Sound Bank");
        summary.AppendLine($"Tracks: {tracks.Count}");
        summary.AppendLine($"Size: {FormatFileSize(data.Length)}");
        if (previewLines.Count > 0)
        {
            summary.AppendLine();
            summary.AppendLine("Tracks:");
            foreach (var line in previewLines)
            {
                summary.AppendLine($"- {line}");
            }

            if (tracks.Count > previewLines.Count)
            {
                summary.AppendLine($"... and {tracks.Count - previewLines.Count} more");
            }
        }

        return new PreviewResult
        {
            IsText = true,
            TextContent = summary.ToString(),
            IsAudio = tracks.Count > 0,
            AudioSounds = tracks.Count > 0 ? tracks : null,
            AssetCategory = EAssetCategory.SoundBank,
            AssetActions = EBulkType.Audio
        };
    }

    private PreviewResult? BuildLazyCriWareCueSheetPreview(
        string cueSheetName,
        AcbReader? acbReader)
    {
        if (acbReader == null || _criWareProvider == null)
            return null;

        if (!acbReader.AtomCueSheetData.TryGetValue("Cue", out var cueTable) ||
            !acbReader.AtomCueSheetData.TryGetValue("CueName", out var cueNameTable))
        {
            return null;
        }

        var allSounds = _criWareProvider.ExtractCriWareSounds(acbReader, cueSheetName);

        var tracks = new List<AudioTrackInfo>();
        foreach (var cueNameRow in cueNameTable)
        {
            if (!cueNameRow.TryGetValue("CueName", out var cueNameObj) || cueNameObj is not string cueName)
                continue;
            if (!cueNameRow.TryGetValue("CueIndex", out var cueIndexObj))
                continue;

            var cueIndex = Convert.ToInt32(cueIndexObj);
            if (cueIndex < 0 || cueIndex >= cueTable.Count)
                continue;

            var cueRow = cueTable[cueIndex];
            var cueId = cueRow.TryGetValue("CueId", out var cueIdObj) ? Convert.ToInt32(cueIdObj) : cueIndex;
            var cueSounds = allSounds
                .Where(s => s.Name == cueName || s.Name.StartsWith(cueName + "_"))
                .ToList();
            var trackCount = Math.Max(1, cueSounds.Count);

            for (int i = 0; i < trackCount; i++)
            {
                var trackName = trackCount == 1 ? cueName : $"{cueName}_{i:D4}";
                tracks.Add(new AudioTrackInfo
                {
                    Name = trackName,
                    Format = "wav",
                    OriginalFormat = "hca",
                    DataLoader = CreateCriWareTrackLoader(cueSounds, i)
                });
            }
        }

        return new PreviewResult
        {
            IsText = true,
            TextContent = $"CriWare Cue Sheet\nTracks: {tracks.Count}\nName: {cueSheetName}",
            IsAudio = tracks.Count > 0,
            AudioSounds = tracks.Count > 0 ? tracks : null,
            AssetCategory = EAssetCategory.Audio,
            AssetActions = EBulkType.Audio
        };
    }

    private static Func<Task<DeferredAudioData?>> CreateAwbTrackLoader(byte[] awbData, ushort waveId)
    {
        return async () =>
        {
            return await Task.Run(() =>
            {
                using var stream = new MemoryStream(awbData, writable: false) { Position = 0 };
                using var awbReader = new AwbReader(stream);
                var wave = awbReader.Waves.FirstOrDefault(w => w.WaveId == waveId);
                if (wave.Length == 0)
                    return null;

                using var waveStream = awbReader.GetWaveSubfileStream(wave);
                var waveData = new byte[wave.Length];
                waveStream.ReadExactly(waveData, 0, waveData.Length);

                try
                {
                    var hcaDataWithSubkey = new byte[waveData.Length + 2];
                    System.Buffer.BlockCopy(waveData, 0, hcaDataWithSubkey, 0, waveData.Length);
                    var subKeyBytes = BitConverter.GetBytes(awbReader.Subkey);
                    System.Buffer.BlockCopy(subKeyBytes, 0, hcaDataWithSubkey, waveData.Length, 2);
                    var wavData = HcaWaveStream.ConvertHcaToWav(hcaDataWithSubkey, 0);

                    return new DeferredAudioData
                    {
                        Data = wavData,
                        Format = "wav",
                        OriginalFormat = "hca"
                    };
                }
                catch
                {
                    return new DeferredAudioData
                    {
                        Data = waveData,
                        Format = "hca",
                        OriginalFormat = "hca"
                    };
                }
            });
        };
    }

    private Func<Task<DeferredAudioData?>> CreateCriWareTrackLoader(
        List<CriWareExtractedSound> cueSounds,
        int trackIndex)
    {
        return async () =>
        {
            if (trackIndex < 0 || trackIndex >= cueSounds.Count)
            {
                return null;
            }

            return await Task.Run(() => ConvertToDeferredAudioData(cueSounds[trackIndex]));
        };
    }

    private static AudioTrackInfo ConvertCriWareSound(CriWareExtractedSound sound)
    {
        var deferred = ConvertToDeferredAudioData(sound);
        return new AudioTrackInfo
        {
            Name = sound.Name,
            Data = deferred.Data,
            Format = deferred.Format,
            OriginalFormat = deferred.OriginalFormat
        };
    }

    private static DeferredAudioData ConvertToDeferredAudioData(CriWareExtractedSound sound)
    {
        byte[] data = sound.Data;
        string format = sound.Extension;

        if (sound.Extension.Equals("hca", StringComparison.OrdinalIgnoreCase))
        {
            try
            {
                var hcaDataWithSubkey = new byte[sound.Data.Length + 2];
                System.Buffer.BlockCopy(sound.Data, 0, hcaDataWithSubkey, 0, sound.Data.Length);
                var wavData = HcaWaveStream.ConvertHcaToWav(hcaDataWithSubkey, 0);
                data = wavData;
                format = "wav";
            }
            catch
            {
            }
        }
        else if (sound.Extension.Equals("adx", StringComparison.OrdinalIgnoreCase))
        {
            try
            {
                var adxDataWithSubkey = new byte[sound.Data.Length + 2];
                System.Buffer.BlockCopy(sound.Data, 0, adxDataWithSubkey, 0, sound.Data.Length);
                var wavData = AdxDecoder.ConvertAdxToWav(adxDataWithSubkey, 0);
                data = wavData;
                format = "wav";
            }
            catch
            {
            }
        }

        return new DeferredAudioData
        {
            Data = data,
            Format = format,
            OriginalFormat = sound.Extension
        };
    }

    private static PreviewResult? TryBuildPackageAudioPreview(CUE4Parse.UE4.Assets.IPackage package, string filePath)
    {
        foreach (var exportLazy in package.ExportsLazy)
        {
            var obj = exportLazy?.Value;
            if (obj is UAkMediaAsset akMediaAsset &&
                TryCreatePackageAudioPreview(akMediaAsset, filePath, out var preview))
            {
                return preview;
            }
        }

        foreach (var exportLazy in package.ExportsLazy)
        {
            var obj = exportLazy?.Value;
            if (obj == null)
                continue;

            if (obj is UAkMediaAssetData mediaData && mediaData.Outer?.Load() is UAkMediaAsset)
                continue;

            if (TryCreatePackageAudioPreview(obj, filePath, out var preview))
            {
                return preview;
            }
        }

        return null;
    }

    private static bool TryCreatePackageAudioPreview(UObject obj, string filePath, out PreviewResult preview)
    {
        switch (obj)
        {
            case UAkMediaAsset akMediaAsset when akMediaAsset.CurrentMediaAssetData?.ResolvedObject?.Object?.Value is UAkMediaAssetData mediaData:
                return TryCreateDecodedAudioPreview(mediaData, akMediaAsset.Name, filePath, akMediaAsset.ExportType, out preview);

            case UAkMediaAssetData mediaData:
                return TryCreateDecodedAudioPreview(mediaData, mediaData.Name, filePath, mediaData.ExportType, out preview);

            case USoundWave soundWave:
                return TryCreateDecodedAudioPreview(soundWave, soundWave.Name, filePath, soundWave.ExportType, out preview);

            default:
                preview = null!;
                return false;
        }
    }

    private static bool TryCreateDecodedAudioPreview(UObject audioObject, string displayName, string filePath, string exportType, out PreviewResult preview)
    {
        audioObject.Decode(true, out var audioFormat, out var decodedData);
        if (decodedData == null || string.IsNullOrWhiteSpace(audioFormat))
        {
            preview = null!;
            return false;
        }

        var normalizedFormat = NormalizeDecodedAudioFormat(audioFormat);
        preview = new PreviewResult
        {
            IsText = true,
            TextContent = "🎵 Audio asset: " + displayName + "\nType: " + exportType + "\nFormat: " + audioFormat.ToUpperInvariant() + "\nPackage: " + Path.GetFileName(filePath),
            IsAudio = true,
            AudioData = decodedData,
            AudioFormat = normalizedFormat,
            AssetCategory = EAssetCategory.Audio,
            AssetActions = EBulkType.Audio
        };
        return true;
    }

    private static string NormalizeDecodedAudioFormat(string audioFormat)
    {
        var normalized = audioFormat.Trim();

        if (normalized.Equals("pcm", StringComparison.OrdinalIgnoreCase) ||
            normalized.Equals("adpcm", StringComparison.OrdinalIgnoreCase) ||
            normalized.Equals("wave", StringComparison.OrdinalIgnoreCase))
        {
            return "wav";
        }

        if (normalized.IndexOf("ogg", StringComparison.OrdinalIgnoreCase) >= 0)
        {
            return "ogg";
        }

        return normalized.ToLowerInvariant();
    }

    private async Task<PreviewResult?> HandleByExtensionAsync(string ext, byte[] data, IFileProvider provider, string filePath)
    {
        var extNoDot = ext.TrimStart('.');

        if (IsTextFile(ext))
        {
            var text = TryReadAsText(data);
            var result = new PreviewResult { IsText = true, TextContent = text };

            result.AssetCategory = extNoDot switch
            {
                "uproject" or "uefnproject" or "upluginmanifest" or "uplugin" or "ini" or "locmeta" or "locres"
                or "verse" or "lua" or "json5" or "json" or "bin" or "txt" or "log" or "pem" or "xml"
                or "gitignore" or "html" or "css" or "js" or "data" or "csv" => EAssetCategory.Data,
                "stinfo" or "ushaderbytecode" or "upipelinecache" => EAssetCategory.ByteCode,
                _ => EAssetCategory.Data,
            };
            return result;
        }

        if (ext == ".ini" && filePath.Contains("BinaryConfig", StringComparison.OrdinalIgnoreCase))
        {
            try
            {
                var gameFile = provider.Files[filePath];
                var archive = gameFile.CreateReader();
                var configCache = new FConfigCacheIni(archive);
                var json = JsonConvert.SerializeObject(configCache, Formatting.Indented);
                return new PreviewResult { IsText = true, TextContent = json, AssetCategory = EAssetCategory.Data };
            }
            catch (Exception ex)
            {
                Log.Warning($"Failed to parse BinaryConfig INI: {ex.Message}");

            }
        }

        if (ext == ".locmeta")
        {
            try
            {
                var gameFile = provider.Files[filePath];
                var archive = gameFile.CreateReader();
                var metadata = new FTextLocalizationMetaDataResource(archive);
                var json = JsonConvert.SerializeObject(metadata, Formatting.Indented);
                return new PreviewResult { IsText = true, TextContent = json, AssetCategory = EAssetCategory.Data };
            }
            catch (Exception ex)
            {
                Log.Warning($"Failed to parse locmeta: {ex.Message}");
            }
        }
        if (ext == ".locres")
        {
            try
            {
                var gameFile = provider.Files[filePath];
                var archive = gameFile.CreateReader();
                var locres = new FTextLocalizationResource(archive);
                var json = JsonConvert.SerializeObject(locres, Formatting.Indented);
                return new PreviewResult { IsText = true, TextContent = json, AssetCategory = EAssetCategory.Data };
            }
            catch (Exception ex)
            {
                Log.Warning($"Failed to parse locres: {ex.Message}");
            }
        }

        if (ext == ".bin")
        {
            try
            {
                var gameFile = provider.Files[filePath];
                if (filePath.Contains("AssetRegistry", StringComparison.OrdinalIgnoreCase))
                {
                    var archive = gameFile.CreateReader();
                    var registry = new FAssetRegistryState(archive);
                    var json = JsonConvert.SerializeObject(registry, Formatting.Indented);
                    return new PreviewResult { IsText = true, TextContent = json, AssetCategory = EAssetCategory.ByteCode };
                }
                if (filePath.Contains("GlobalShaderCache", StringComparison.OrdinalIgnoreCase))
                {
                    var archive = gameFile.CreateReader();
                    var registry = new FGlobalShaderCache(archive);
                    var json = JsonConvert.SerializeObject(registry, Formatting.Indented);
                    return new PreviewResult { IsText = true, TextContent = json, AssetCategory = EAssetCategory.ByteCode };
                }
            }
            catch (Exception ex)
            {
                Log.Warning($"Failed to parse .bin file: {ex.Message}");
            }
        }

        if (ext == ".udic")
        {
            try
            {
                var gameFile = provider.Files[filePath];
                var archive = gameFile.CreateReader();
                var header = new FOodleDictionaryArchive(archive).Header;
                var json = JsonConvert.SerializeObject(header, Formatting.Indented);
                return new PreviewResult { IsText = true, TextContent = json, AssetCategory = EAssetCategory.ByteCode };
            }
            catch (Exception ex)
            {
                Log.Warning($"Failed to parse .udic: {ex.Message}");
            }
        }

        if (ext == ".ushaderbytecode" || ext == ".ushadercode")
        {
            try
            {
                var gameFile = provider.Files[filePath];
                var archive = gameFile.CreateReader();
                var ar = new FShaderCodeArchive(archive);
                var json = JsonConvert.SerializeObject(ar, Formatting.Indented);
                return new PreviewResult { IsText = true, TextContent = json, AssetCategory = EAssetCategory.ByteCode };
            }
            catch (Exception ex)
            {
                Log.Warning($"Failed to parse shader archive: {ex.Message}");
            }
        }
        if (ext == ".upipelinecache")
        {
            try
            {
                var gameFile = provider.Files[filePath];
                var archive = gameFile.CreateReader();
                var ar = new FPipelineCacheFile(archive);
                var json = JsonConvert.SerializeObject(ar, Formatting.Indented);
                return new PreviewResult { IsText = true, TextContent = json, AssetCategory = EAssetCategory.ByteCode };
            }
            catch (Exception ex)
            {
                Log.Warning($"Failed to parse pipeline cache: {ex.Message}");
            }
        }
        if (ext == ".stinfo")
        {
            try
            {
                var gameFile = provider.Files[filePath];
                var archive = gameFile.CreateReader();
                var ar = new FShaderTypeHashes(archive);
                var json = JsonConvert.SerializeObject(ar, Formatting.Indented);
                return new PreviewResult { IsText = true, TextContent = json, AssetCategory = EAssetCategory.ByteCode };
            }
            catch (Exception ex)
            {
                Log.Warning($"Failed to parse stinfo: {ex.Message}");
            }
        }

        if (ext is ".wav" or ".ogg" or ".flac" or ".mp4" or ".opus" or ".mp3" or ".mid")
        {
            return new PreviewResult
            {
                IsAudio = true,
                AudioData = data,
                AudioFormat = extNoDot,
                TextContent = $"🎵 Audio file ({FormatFileSize(data.Length)})\nFormat: {extNoDot}",
                AssetCategory = EAssetCategory.Audio,
                AssetActions = EBulkType.Audio
            };
        }
        if (ext == ".wem")
        {

            try
            {
                var gameFile = provider.Files[filePath];
                var archive = gameFile.CreateReader();
                var wwise = new WwiseReader(new FWwiseArchive(archive), new WwiseGameFileSource(gameFile));
                var wemData = wwise.WemFile?.GetData();
                if (wemData != null && wemData.Length > 0)
                {
                    return new PreviewResult
                    {
                        IsAudio = true,
                        AudioData = wemData,
                        AudioFormat = "wem",
                        TextContent = $"🎵 Wwise Audio ({FormatFileSize(wemData.Length)})\nFormat: WEM",
                        AssetCategory = EAssetCategory.Audio,
                        AssetActions = EBulkType.Audio
                    };
                }
            }
            catch (Exception ex)
            {
                Log.Warning($"Failed to parse WEM: {ex.Message}");
            }

            return new PreviewResult
            {
                IsAudio = true,
                AudioData = data,
                AudioFormat = "wem",
                TextContent = $"🎵 Wwise Audio ({FormatFileSize(data.Length)})\nFormat: WEM (raw)",
                AssetCategory = EAssetCategory.Audio,
                AssetActions = EBulkType.Audio
            };
        }
        if (ext is ".xvag" or ".at9")
        {
            return new PreviewResult
            {
                IsAudio = true,
                AudioData = data,
                AudioFormat = extNoDot,
                TextContent = $"🎵 Audio file ({FormatFileSize(data.Length)})\nFormat: {extNoDot} (PlayStation)",
                AssetCategory = EAssetCategory.Audio,
                AssetActions = EBulkType.Audio
            };
        }

        if (ext == ".hca")
        {
            try
            {

                var hcaDataWithSubkey = new byte[data.Length + 2];
                System.Buffer.BlockCopy(data, 0, hcaDataWithSubkey, 0, data.Length);

                var wavData = HcaWaveStream.ConvertHcaToWav(hcaDataWithSubkey, 0);
                return new PreviewResult
                {
                    IsAudio = true,
                    AudioData = wavData,
                    AudioFormat = "wav",
                    TextContent = $"🎵 CriWare HCA Audio ({FormatFileSize(data.Length)})\nFormat: HCA → WAV",
                    AssetCategory = EAssetCategory.Audio,
                    AssetActions = EBulkType.Audio
                };
            }
            catch
            {

                return new PreviewResult
                {
                    IsAudio = true,
                    AudioData = data,
                    AudioFormat = "hca",
                    TextContent = $"🎵 CriWare HCA Audio ({FormatFileSize(data.Length)})\nFormat: HCA (encrypted, needs key)",
                    AssetCategory = EAssetCategory.Audio,
                    AssetActions = EBulkType.Audio
                };
            }
        }

        if (ext == ".adx")
        {
            try
            {

                var adxDataWithSubkey = new byte[data.Length + 2];
                System.Buffer.BlockCopy(data, 0, adxDataWithSubkey, 0, data.Length);
                var wavData = AdxDecoder.ConvertAdxToWav(adxDataWithSubkey, 0);
                return new PreviewResult
                {
                    IsAudio = true,
                    AudioData = wavData,
                    AudioFormat = "wav",
                    TextContent = $"🎵 CriWare ADX Audio ({FormatFileSize(data.Length)})\nFormat: ADX → WAV",
                    AssetCategory = EAssetCategory.Audio,
                    AssetActions = EBulkType.Audio
                };
            }
            catch
            {
                return new PreviewResult
                {
                    IsAudio = true,
                    AudioData = data,
                    AudioFormat = "adx",
                    TextContent = $"🎵 CriWare ADX Audio ({FormatFileSize(data.Length)})\nFormat: ADX (encrypted, needs key)",
                    AssetCategory = EAssetCategory.Audio,
                    AssetActions = EBulkType.Audio
                };
            }
        }

        if (ext == ".bnk" || ext == ".pck")
        {
            try
            {
                var gameFile = provider.Files[filePath];
                var archive = gameFile.CreateReader();
                var wwise = new WwiseReader(new FWwiseArchive(archive), new WwiseGameFileSource(gameFile));
                var json = JsonConvert.SerializeObject(wwise, Formatting.Indented);

                var sounds = new List<AudioTrackInfo>();
                if (wwise.WemIndexes != null)
                {
                    for (int i = 0; i < wwise.WemIndexes.Length; i++)
                    {
                        var mediaHeader = wwise.WemIndexes[i];
                        var key = mediaHeader.Id.ToString();
                        if (wwise.WwiseEncodedMedias.TryGetValue(key, out var deferredData))
                        {
                            var wemData = deferredData.GetData();
                            if (wemData.Length > 0)
                            {
                                sounds.Add(new AudioTrackInfo
                                {
                                    Name = wwise.WemIndexes?.Length > i ? $"WEM_{wwise.WemIndexes[i].Id}" : $"Track_{i}",
                                    Data = wemData,
                                    Format = "wem",
                                    OriginalFormat = "wem"
                                });
                            }
                        }
                    }
                }

                if (sounds.Count == 0 && wwise.WwiseEncodedMedias.Count > 0)
                {
                    foreach (var kvp in wwise.WwiseEncodedMedias)
                    {
                        var wemData = kvp.Value.GetData();
                        if (wemData.Length > 0)
                        {
                            sounds.Add(new AudioTrackInfo
                            {
                                Name = kvp.Key,
                                Data = wemData,
                                Format = "wem",
                                OriginalFormat = "wem"
                            });
                        }
                    }
                }

                return new PreviewResult
                {
                    IsText = true,
                    TextContent = json,
                    IsAudio = sounds.Count > 0,
                    AudioSounds = sounds.Count > 0 ? sounds : null,
                    AssetCategory = EAssetCategory.SoundBank,
                    AssetActions = EBulkType.Audio
                };
            }
            catch (Exception ex)
            {
                Log.Warning($"Failed to parse Wwise bank: {ex.Message}");
                return new PreviewResult
                {
                    IsText = true,
                    TextContent = $"Wwise Sound Bank ({FormatFileSize(data.Length)})\nParse error: {ex.Message}\n\nUse Export to save raw data.",
                    AssetCategory = EAssetCategory.SoundBank,
                    AssetActions = EBulkType.Audio
                };
            }
        }

        if (ext == ".bank")
        {
            try
            {
                var gameFile = provider.Files[filePath];
                using var stream = new MemoryStream(data) { Position = 0 };
                try
                {
                    using var reader = new BinaryReader(stream);
                    var fmodReader = new FModReader(reader, Path.GetFileNameWithoutExtension(filePath));
                    var json = JsonConvert.SerializeObject(fmodReader, Formatting.Indented);
                    return new PreviewResult
                    {
                        IsText = true,
                        TextContent = json,
                        AssetCategory = EAssetCategory.SoundBank,
                        AssetActions = EBulkType.Audio
                    };
                }
                catch (Exception ex)
                {
                    Log.Error(ex, "Can't load FMOD bank");
                }
            }
            catch (Exception ex)
            {
                Log.Warning($"Failed to parse FMOD bank: {ex.Message}");
            }
            return new PreviewResult
            {
                IsText = true,
                TextContent = $"FMOD Sound Bank ({FormatFileSize(data.Length)})\nUse Export to save raw data.",
                AssetCategory = EAssetCategory.SoundBank,
                AssetActions = EBulkType.Audio
            };
        }

        if (ext == ".acb")
        {
            try
            {
                using var stream = new MemoryStream(data) { Position = 0 };
                var acbReader = new AcbReader(stream);
                var json = JsonConvert.SerializeObject(acbReader, Formatting.Indented);
                return new PreviewResult
                {
                    IsText = true,
                    TextContent = json,
                    AssetCategory = EAssetCategory.SoundBank,
                    AssetActions = EBulkType.Audio
                };
            }
            catch (Exception ex)
            {
                Log.Warning($"Failed to parse ACB: {ex.Message}");
            }
        }

        if (ext == ".png" || ext == ".jpg" || ext == ".bmp")
        {
            try
            {
                using var stream = new MemoryStream(data) { Position = 0 };
                using var bitmap = SKBitmap.Decode(stream);
                if (bitmap != null)
                {
                    using var image = SKImage.FromBitmap(bitmap);
                    using var encoded = image.Encode(ext == ".jpg" ? SKEncodedImageFormat.Jpeg : SKEncodedImageFormat.Png, 100);
                    var ms = new MemoryStream();
                    encoded.SaveTo(ms);
                    ms.Position = 0;

                    var dispatcher = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();
                    var bmp = await CreateBitmapImageOnUiThreadAsync(ms, dispatcher);
                    if (bmp != null)
                    {
                        return new PreviewResult
                        {
                            Image = bmp,
                            IsTexture = true,
                            AssetCategory = EAssetCategory.Texture,
                            AssetActions = EBulkType.Textures
                        };
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Warning($"Failed to decode standalone image: {ex.Message}");
            }
            return new PreviewResult
            {
                IsText = true,
                TextContent = $"Image file ({FormatFileSize(data.Length)})\nUse Export to save.",
                AssetCategory = EAssetCategory.Texture,
                AssetActions = EBulkType.Textures
            };
        }
        if (ext == ".svg")
        {
            try
            {
                using var stream = new MemoryStream(data) { Position = 0 };
                var svg = new SKSvg();
                svg.Load(stream);
                if (svg.Picture != null)
                {
                    var size = 512;
                    using var bitmap = new SKBitmap(size, size);
                    using var canvas = new SKCanvas(bitmap);
                    canvas.Clear(SKColors.Transparent);
                    var bounds = svg.Picture.CullRect;
                    float scale = Math.Min(size / bounds.Width, size / bounds.Height);
                    canvas.Scale(scale);
                    canvas.Translate(-bounds.Left, -bounds.Top);
                    canvas.DrawPicture(svg.Picture);

                    using var image = SKImage.FromBitmap(bitmap);
                    using var encoded = image.Encode(SKEncodedImageFormat.Png, 100);
                    var ms = new MemoryStream();
                    encoded.SaveTo(ms);
                    ms.Position = 0;

                    var dispatcher = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();
                    var bmp = await CreateBitmapImageOnUiThreadAsync(ms, dispatcher);
                    if (bmp != null)
                    {
                        return new PreviewResult
                        {
                            Image = bmp,
                            IsTexture = true,
                            AssetCategory = EAssetCategory.Texture,
                            AssetActions = EBulkType.Textures
                        };
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Warning($"Failed to decode SVG: {ex.Message}");
            }
            return new PreviewResult
            {
                IsText = true,
                TextContent = $"SVG file ({FormatFileSize(data.Length)})\nUse Export to save.",
                AssetCategory = EAssetCategory.Texture,
                AssetActions = EBulkType.Textures
            };
        }

        if (ext == ".ufont" || ext == ".otf" || ext == ".ttf")
        {
            return new PreviewResult
            {
                IsText = true,
                TextContent = $"🔤 Font file ({FormatFileSize(data.Length)})\n" +
                             $"Extension: {ext}\n\n" +
                             $"Export raw data and change extension if you want it to be an installable font file.",
                AssetCategory = EAssetCategory.Font
            };
        }

        if (ext == ".mp4")
        {
            return new PreviewResult
            {
                IsText = true,
                TextContent = $"🎬 Video file ({FormatFileSize(data.Length)})\nUse Export to save.",
                AssetCategory = EAssetCategory.Video
            };
        }

        if (ext == ".luac")
        {
            return new PreviewResult
            {
                IsText = true,
                TextContent = $"Compiled Lua bytecode ({FormatFileSize(data.Length)})\nNot human-readable. Use Export to save raw data.",
                AssetCategory = EAssetCategory.Data
            };
        }

        if (ext == ".res")
        {
            return new PreviewResult
            {
                IsText = true,
                TextContent = $"Resource file (skipped)\n{FormatFileSize(data.Length)}",
                AssetCategory = EAssetCategory.All
            };
        }

        return null;
    }

    private static async Task<BitmapImage?> CreateBitmapImageOnUiThreadAsync(
        MemoryStream stream,
        Microsoft.UI.Dispatching.DispatcherQueue? dispatcher)
    {
        if (dispatcher == null)
        {
            stream.Dispose();
            return null;
        }

        if (dispatcher.HasThreadAccess)
        {
            try
            {
                var bitmapImage = new BitmapImage();
                bitmapImage.SetSource(stream.AsRandomAccessStream());
                return bitmapImage;
            }
            catch
            {
                stream.Dispose();
                return null;
            }
        }

        var tcs = new TaskCompletionSource<BitmapImage?>();
        dispatcher.TryEnqueue(() =>
        {
            try
            {
                var bitmapImage = new BitmapImage();
                bitmapImage.SetSource(stream.AsRandomAccessStream());
                tcs.SetResult(bitmapImage);
            }
            catch
            {
                stream.Dispose();
                tcs.SetResult(null);
            }
        });

        return await tcs.Task;
    }

    private static void FinalizePreviewResult(PreviewResult result)
    {
        if (!result.IsText || string.IsNullOrEmpty(result.TextContent))
            return;

        result.TextContent = NormalizeLineEndings(result.TextContent);
        result.TextLineCount = CountLines(result.TextContent);
    }

    private string TryReadAsText(byte[] data)
    {
        if (data.Length == 0)
            return string.Empty;

        try
        {
            string? bestText = null;
            double bestScore = double.MinValue;

            foreach (var encoding in GetCandidateEncodings(data))
            {
                if (!TryDecodeText(data, encoding, out var decoded))
                    continue;

                var score = GetTextQualityScore(decoded);
                if (score > bestScore)
                {
                    bestScore = score;
                    bestText = decoded;
                }
            }

            if (bestText == null || bestScore < 0.8)
                return "[Binary file - cannot display as text]";

            return bestText;
        }
        catch
        {
            return "[Error reading file as text]";
        }
    }

    private static IEnumerable<Encoding> GetCandidateEncodings(byte[] data)
    {
        var encodings = new List<Encoding>();
        var yielded = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        void YieldEncoding(Encoding encoding)
        {
            if (yielded.Add(encoding.WebName))
                encodings.Add(encoding);
        }

        if (HasBom(data, 0xEF, 0xBB, 0xBF)) YieldEncoding(new UTF8Encoding(true, true));
        if (HasBom(data, 0xFF, 0xFE, 0x00, 0x00)) YieldEncoding(new UTF32Encoding(false, true, true));
        if (HasBom(data, 0x00, 0x00, 0xFE, 0xFF)) YieldEncoding(new UTF32Encoding(true, true, true));
        if (HasBom(data, 0xFF, 0xFE)) YieldEncoding(new System.Text.UnicodeEncoding(false, true, true));
        if (HasBom(data, 0xFE, 0xFF)) YieldEncoding(new System.Text.UnicodeEncoding(true, true, true));

        YieldEncoding(new UTF8Encoding(false, true));
        YieldEncoding(new System.Text.UnicodeEncoding(false, true, true));
        YieldEncoding(new System.Text.UnicodeEncoding(true, true, true));
        YieldEncoding(new UTF32Encoding(false, true, true));
        YieldEncoding(Encoding.Latin1);
        YieldEncoding(Encoding.Default);

        return encodings;
    }

    private static bool HasBom(byte[] data, params byte[] bom)
    {
        if (data.Length < bom.Length)
            return false;

        for (int i = 0; i < bom.Length; i++)
        {
            if (data[i] != bom[i])
                return false;
        }

        return true;
    }

    private static bool TryDecodeText(byte[] data, Encoding encoding, out string text)
    {
        try
        {
            text = encoding.GetString(data);
            return true;
        }
        catch
        {
            text = string.Empty;
            return false;
        }
    }

    private static double GetTextQualityScore(string text)
    {
        if (string.IsNullOrEmpty(text))
            return 1.0;

        int printableCount = 0;
        foreach (var c in text)
        {
            if (!char.IsControl(c) || char.IsWhiteSpace(c))
                printableCount++;
        }

        return (double) printableCount / text.Length;
    }

    private static string NormalizeLineEndings(string text)
    {
        return text.Replace("\r\n", "\n").Replace('\r', '\n');
    }

    private static int CountLines(string text)
    {
        if (string.IsNullOrEmpty(text))
            return 0;

        int lineCount = 1;
        foreach (var c in text)
        {
            if (c == '\n')
                lineCount++;
        }

        return lineCount;
    }

    private MemoryStream? LoadTextureStream(UTexture2D texture)
    {
        try
        {

            var decoded = texture.Decode(UserSettings.Default.ExportOptions);
            if (decoded == null)
            {
                Log.Warning("Texture.Decode() returned null");
                return null;
            }

            using var bitmap = decoded.ToSkBitmap();
            if (bitmap == null)
            {
                Log.Warning("ToSkBitmap() returned null");
                return null;
            }

            var ms = new MemoryStream();
            if (!bitmap.Encode(ms, SKEncodedImageFormat.Png, 100))
            {
                ms.Dispose();
                return null;
            }

            ms.Position = 0;
            return ms;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to decode texture");
            return null;
        }
    }

    private async Task<PreviewResult> LoadMaterialAsync(IFileProvider provider, UObject material, byte[] rawData)
    {
        try
        {
            Log.Information($"Loading material: {material.Name} (Type: {material.GetType().Name})");

            BitmapImage? baseColorImage = null;
            var materialData = new MaterialPreviewData();

            if (material is UMaterialInstanceConstant matInst)
            {
                Log.Information($"Processing UMaterialInstanceConstant with {matInst.TextureParameterValues?.Length ?? 0} texture params");
                baseColorImage = await ExtractBaseColorTextureAsync(matInst);
                await ExtractMaterialParametersAsync(matInst, materialData);

                if (materialData.TextureParams.Count == 0 &&
                    materialData.ScalarParams.Count == 0 &&
                    materialData.VectorParams.Count == 0 &&
                    matInst.Parent != null)
                {
                    Log.Information($"Material has no overrides, extracting from parent: {matInst.Parent.Name}");

                    if (matInst.Parent != null && matInst.Parent.TryLoad(out UMaterialInstanceConstant parentMatInst))
                    {
                        baseColorImage = await ExtractBaseColorTextureAsync(parentMatInst);
                        await ExtractMaterialParametersAsync(parentMatInst, materialData);
                    }
                }
            }
            else if (material is UMaterialInstanceDynamic matDyn)
            {
                Log.Information("Processing UMaterialInstanceDynamic");
                if (matDyn.TryGetValue(out FPackageIndex parent, "Parent") && parent.TryLoad(out UMaterialInstanceConstant parentMat))
                {
                    baseColorImage = await ExtractBaseColorTextureAsync(parentMat);
                    await ExtractMaterialParametersAsync(parentMat, materialData);
                }
            }
            else if (material is UMaterial mat)
            {
                Log.Information("Processing UMaterial");
                baseColorImage = await ExtractBaseColorFromMaterialAsync(mat);
            }

            Log.Information($"Material loaded with {materialData.TextureParams.Count} textures, {materialData.ScalarParams.Count} scalars, {materialData.VectorParams.Count} vectors");

            var materialJson = JsonConvert.SerializeObject(material, Formatting.Indented, new JsonSerializerSettings
            {
                ReferenceLoopHandling = ReferenceLoopHandling.Ignore,
                NullValueHandling = NullValueHandling.Ignore
            });

            return new PreviewResult
            {
                Image = baseColorImage,
                IsText = false,
                TextContent = materialJson,
                MaterialData = materialData
            };
        }
        catch (Exception ex)
        {
            Log.Warning($"Failed to load material: {ex.Message}");
            return new PreviewResult { IsText = true, TextContent = $"Material load error: {ex.Message}" };
        }
    }

    private async Task<BitmapImage?> ExtractBaseColorTextureAsync(UMaterialInstanceConstant material)
    {
        try
        {
            foreach (var textureParam in material.TextureParameterValues)
            {
                var paramName = textureParam.ParameterInfo.Name.Text;
                if (paramName.Equals("BaseColor", StringComparison.OrdinalIgnoreCase) ||
                    paramName.Equals("Diffuse", StringComparison.OrdinalIgnoreCase) ||
                    paramName.Equals("BaseColorTexture", StringComparison.OrdinalIgnoreCase))
                {
                    if (textureParam.ParameterValue.TryLoad(out UTexture2D texture))
                    {
                        return await ConvertTextureToBitmapAsync(texture, 512);
                    }
                }
            }

            foreach (var textureParam in material.TextureParameterValues)
            {
                if (textureParam.ParameterValue.TryLoad(out UTexture2D texture))
                {
                    return await ConvertTextureToBitmapAsync(texture, 512);
                }
            }
        }
        catch (Exception ex)
        {
            Log.Warning($"Failed to extract BaseColor: {ex.Message}");
        }
        return null;
    }

    private async Task<BitmapImage?> ExtractBaseColorFromMaterialAsync(UMaterial material)
    {
        try
        {
            if (material.TryGetValue(out FPackageIndex[] referencedTextures, "ReferencedTextures"))
            {
                foreach (var texRef in referencedTextures)
                {
                    if (texRef.TryLoad(out UTexture2D texture))
                    {
                        return await ConvertTextureToBitmapAsync(texture, 512);
                    }
                }
            }
        }
        catch (Exception ex)
        {
            Log.Warning($"Failed to extract texture from material: {ex.Message}");
        }
        return null;
    }

    private async Task<BitmapImage?> ConvertTextureToBitmapAsync(UTexture2D texture, int maxSize = 384)
    {
        try
        {

            var decoded = texture.Decode(UserSettings.Default.ExportOptions);
            if (decoded == null) return null;

            using var skBitmap = decoded.ToSkBitmap();
            if (skBitmap == null) return null;

            MemoryStream ms;
            var width = skBitmap.Width;
            var height = skBitmap.Height;
            if (width > maxSize || height > maxSize)
            {
                var scale = Math.Min((double)maxSize / width, (double)maxSize / height);
                var newW = Math.Max(1, (int)(width * scale));
                var newH = Math.Max(1, (int)(height * scale));
                using var resized = new SKBitmap(newW, newH);
                skBitmap.ScalePixels(resized, SKSamplingOptions.Default);
                using var imageRes = SKImage.FromBitmap(resized);
                using var dataRes = imageRes.Encode(SKEncodedImageFormat.Png, 90);
                ms = new MemoryStream();
                dataRes.SaveTo(ms);
                ms.Position = 0;
            }
            else
            {
                using var image = SKImage.FromBitmap(skBitmap);
                using var data = image.Encode(SKEncodedImageFormat.Png, 90);
                ms = new MemoryStream();
                data.SaveTo(ms);
                ms.Position = 0;
            }

            var dispatcher = Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread();
            if (dispatcher != null)
            {
                var tcs = new TaskCompletionSource<BitmapImage?>();
                dispatcher.TryEnqueue(() =>
                {
                    try
                    {
                        var bitmapImage = new BitmapImage();
                        bitmapImage.SetSource(ms.AsRandomAccessStream());
                        tcs.SetResult(bitmapImage);
                    }
                    catch (Exception ex)
                    {
                        ms.Dispose();
                        Log.Error(ex, "Failed to create BitmapImage on UI thread");
                        tcs.SetResult(null);
                    }
                });
                return await tcs.Task;
            }
            else
            {
                ms.Dispose();
                return null;
            }
        }
        catch (Exception ex)
        {
            Log.Warning($"Failed to convert texture: {ex.Message}");
            return null;
        }
    }

    private async Task ExtractMaterialParametersAsync(UMaterialInstanceConstant material, MaterialPreviewData data)
    {
        if (material.TextureParameterValues != null && material.TextureParameterValues.Length > 0)
        {
            foreach (var param in material.TextureParameterValues)
            {
                var textureParam = new MaterialTextureParam
                {
                    Name = param.ParameterInfo.Name.Text
                };

                if (param.ParameterValue.TryLoad(out UTexture2D tex))
                {
                    textureParam.Path = tex.GetPathName();
                    textureParam.Thumbnail = await ConvertTextureToBitmapAsync(tex);
                }

                data.TextureParams.Add(textureParam);
            }
        }

        if (material.ScalarParameterValues != null && material.ScalarParameterValues.Length > 0)
        {
            foreach (var param in material.ScalarParameterValues)
            {
                data.ScalarParams.Add(new MaterialScalarParam
                {
                    Name = param.ParameterInfo.Name.Text,
                    Value = param.ParameterValue
                });
            }
        }

        if (material.VectorParameterValues != null && material.VectorParameterValues.Length > 0)
        {
            foreach (var param in material.VectorParameterValues)
            {
                if (param.ParameterValue != null)
                {
                    var vec = param.ParameterValue.Value;
                    data.VectorParams.Add(new MaterialVectorParam
                    {
                        Name = param.ParameterInfo.Name.Text,
                        R = vec.R,
                        G = vec.G,
                        B = vec.B,
                        A = vec.A
                    });
                }
            }
        }
    }

    private string FormatFileSize(long bytes)
    {
        string[] sizes = { "B", "KB", "MB", "GB", "TB" };
        double len = bytes;
        int order = 0;
        while (len >= 1024 && order < sizes.Length - 1)
        {
            order++;
            len /= 1024;
        }
        return $"{len:0.##} {sizes[order]}";
    }
}

public class PreviewResult : IDisposable
{
    public bool IsText { get; set; }
    public string? TextContent { get; set; }
    public int TextLineCount { get; set; }
    public BitmapImage? Image { get; set; }
    public bool IsModel { get; set; }
    public string? ModelName { get; set; }
    public bool IsTexture { get; set; }
    public bool IsAudio { get; set; }
    public byte[]? AudioData { get; set; }
    public string? AudioFormat { get; set; }
    public List<AudioTrackInfo>? AudioSounds { get; set; }

    public MaterialPreviewData? MaterialData { get; set; }
    public EAssetCategory AssetCategory { get; set; } = EAssetCategory.All;
    public EBulkType AssetActions { get; set; } = EBulkType.None;

    public void Dispose()
    {

        Image = null;
        MaterialData?.Dispose();
        MaterialData = null;
        AudioData = null;
        AudioSounds = null;
    }
}

public class AudioTrackInfo
{
    public string Name { get; set; } = "";
    public byte[]? Data { get; set; }
    public string Format { get; set; } = "wav";
    public string? OriginalFormat { get; set; }
    public Func<Task<DeferredAudioData?>>? DataLoader { get; set; }
}

public class DeferredAudioData
{
    public byte[] Data { get; set; } = [];
    public string Format { get; set; } = "wav";
    public string? OriginalFormat { get; set; }
}

public class MaterialPreviewData : IDisposable
{

    public List<MaterialTextureParam> TextureParams { get; set; } = new();
    public List<MaterialScalarParam> ScalarParams { get; set; } = new();
    public List<MaterialVectorParam> VectorParams { get; set; } = new();

    public List<MaterialGroup> Materials { get; set; } = new();

    public void Dispose()
    {

        foreach (var param in TextureParams)
        {
            param.Thumbnail = null;
        }
        TextureParams.Clear();
        ScalarParams.Clear();
        VectorParams.Clear();

        foreach (var material in Materials)
        {
            foreach (var tex in material.Textures)
            {
                tex.Thumbnail = null;
            }
            material.Textures.Clear();
            material.Scalars.Clear();
            material.Colors.Clear();
        }
        Materials.Clear();
    }
}

public class MaterialGroup
{
    public string Name { get; set; } = "";
    public int MaterialIndex { get; set; }
    public bool IsExpanded { get; set; } = true;

    public int TextureCount { get; set; }
    public int ScalarCount { get; set; }
    public int ColorCount { get; set; }

    public List<MaterialTextureParam> Textures { get; set; } = new();
    public List<MaterialScalarParam> Scalars { get; set; } = new();
    public List<MaterialVectorParam> Colors { get; set; } = new();
}

public class MaterialTextureParam
{
    public string Name { get; set; } = "";
    public BitmapImage? Thumbnail { get; set; }
    public string Path { get; set; } = "";
    public List<string> MaterialNames { get; set; } = new();
}

public class MaterialScalarParam
{
    public string Name { get; set; } = "";
    public float Value { get; set; }
}

public class MaterialVectorParam
{
    public string Name { get; set; } = "";
    public float R { get; set; }
    public float G { get; set; }
    public float B { get; set; }
    public float A { get; set; }
}
