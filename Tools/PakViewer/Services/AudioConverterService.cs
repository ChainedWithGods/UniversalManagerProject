using System;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;
using CUE4Parse.UE4.Criware.Decoders.HCA;
using Serilog;
using Ww2Ogg.Core;

namespace PakViewer.Services;

public static class AudioConverterService
{

    private const ushort FMT_PCM = 0x0001;
    private const ushort FMT_ADPCM = 0x0002;
    private const ushort FMT_XMA = 0x0165;
    private const ushort FMT_WWISE_OPUS = 0x0166;
    private const ushort FMT_EXTENSIBLE = 0xFFFE;
    private const ushort FMT_VORBIS_OLD = 0x0042;
    private const ushort FMT_VORBIS = 0xFFFF;

    public static readonly string[] ConvertibleExtensions =
    [
        "wem", "hca", "adx"
    ];

    public static bool IsConvertibleFormat(string format)
    {
        var ext = format.TrimStart('.').ToLowerInvariant();
        return Array.Exists(ConvertibleExtensions, e => e == ext);
    }

    public static Task<ConversionResult?> TryConvertAsync(byte[] audioData, string originalExtension)
    {
        var ext = originalExtension.TrimStart('.').ToLowerInvariant();
        return ext switch
        {
            "wem" => ConvertWemAsync(audioData),
            "hca" => ConvertHcaAsync(audioData),
            "adx" => ConvertAdxAsync(audioData),
            _ => Task.FromResult<ConversionResult?>(null)
        };
    }

    private static Task<ConversionResult?> ConvertWemAsync(byte[] audioData)
    {
        try
        {

            if (audioData.Length < 12 ||
                audioData[0] != 'R' || audioData[1] != 'I' || audioData[2] != 'F' || audioData[3] != 'F')
            {
                Log.Warning("WEM file does not have RIFF header, cannot determine codec");
                return TryVgmstreamFallbackAsync(audioData, "wem");
            }

            var fmtOffset = FindChunkOffset(audioData, "fmt ");
            if (fmtOffset < 0 || fmtOffset + 20 > audioData.Length)
            {
                Log.Warning("WEM file has no fmt chunk, cannot determine codec");
                return TryVgmstreamFallbackAsync(audioData, "wem");
            }

            var formatTag = BitConverter.ToUInt16(audioData, fmtOffset + 8);
            var channels = BitConverter.ToUInt16(audioData, fmtOffset + 10);
            var sampleRate = BitConverter.ToUInt32(audioData, fmtOffset + 12);
            var hasVorbChunk = FindChunkOffset(audioData, "vorb") >= 0;

            Log.Information("WEM codec: formatTag=0x{FormatTag:X4}, channels={Channels}, sampleRate={SampleRate}, hasVorbChunk={HasVorb}",
                formatTag, channels, sampleRate, hasVorbChunk);

            switch (formatTag)
            {
                case FMT_PCM:
                    Log.Information("WEM contains PCM data, playing as WAV ({Size} bytes)", audioData.Length);
                    return Task.FromResult<ConversionResult?>(new ConversionResult(audioData, "wav"));

                case FMT_VORBIS_OLD:
                case FMT_VORBIS when hasVorbChunk:
                    return ConvertWemVorbisAsync(audioData);

                case FMT_EXTENSIBLE:
                {

                    var subFormatOffset = fmtOffset + 8 + 2 + 2 + 4 + 4 + 2 + 2 + 2 + 2 + 4;
                    if (subFormatOffset + 16 <= audioData.Length)
                    {
                        var subFormatGuid = new Guid(audioData[subFormatOffset..(subFormatOffset + 16)]);
                        Log.Information("WEM contains 0xFFFE (EXTENSIBLE) with SubFormat {SubFormat}, requires vgmstream for decoding", subFormatGuid);
                    }
                    else
                    {
                        Log.Information("WEM contains 0xFFFE (EXTENSIBLE), requires vgmstream for decoding");
                    }
                    return TryVgmstreamFallbackAsync(audioData, "wem");
                }

                case FMT_WWISE_OPUS:
                    Log.Information("WEM contains Wwise Opus (0x0166), requires vgmstream for decoding");
                    return TryVgmstreamFallbackAsync(audioData, "wem");

                case FMT_XMA:
                    Log.Information("WEM contains XMA (0x0165), requires vgmstream for decoding");
                    return TryVgmstreamFallbackAsync(audioData, "wem");

                case FMT_ADPCM:
                    Log.Information("WEM contains ADPCM (0x0002), requires vgmstream for decoding");
                    return TryVgmstreamFallbackAsync(audioData, "wem");

                default:
                    Log.Warning("WEM has unknown codec formatTag=0x{FormatTag:X4}, trying vgmstream fallback", formatTag);
                    return TryVgmstreamFallbackAsync(audioData, "wem");
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to convert WEM");
            return Task.FromResult<ConversionResult?>(null);
        }
    }

    private static Task<ConversionResult?> ConvertWemVorbisAsync(byte[] audioData)
    {
        try
        {
            using var input = new MemoryStream(audioData);
            using var output = new MemoryStream();

            var converter = new WwiseRiffVorbis(input, CodebookLibrary.Default);
            converter.GenerateOgg(output);

            var oggData = output.ToArray();
            if (oggData.Length == 0)
            {
                Log.Warning("Ww2Ogg produced empty output for Vorbis WEM");
                return Task.FromResult<ConversionResult?>(null);
            }

            Log.Information("Converted WEM Vorbis to OGG ({InSize} -> {OutSize} bytes)",
                audioData.Length, oggData.Length);
            return Task.FromResult<ConversionResult?>(new ConversionResult(oggData, "ogg"));
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to convert WEM Vorbis to OGG via Ww2Ogg");
            return Task.FromResult<ConversionResult?>(null);
        }
    }

    private static async Task<ConversionResult?> TryVgmstreamFallbackAsync(byte[] audioData, string ext)
    {
        var vgmPath = TryGetVgmstreamPath();
        if (vgmPath == null)
        {
            Log.Warning("WEM codec requires vgmstream for decoding, but vgmstream-cli.exe was not found. " +
                        "To play this file, place vgmstream-cli.exe in the app directory.");
            return null;
        }

        var tempInput = Path.Combine(Path.GetTempPath(), $"pakviewer_audio_{Guid.NewGuid():N}.{ext}");
        var tempOutput = Path.ChangeExtension(tempInput, ".wav");

        try
        {
            await File.WriteAllBytesAsync(tempInput, audioData);

            using var process = Process.Start(new ProcessStartInfo
            {
                FileName = vgmPath,
                Arguments = $"-o \"{tempOutput}\" \"{tempInput}\"",
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            });

            if (process == null)
            {
                Log.Error("Failed to start vgmstream process");
                return null;
            }

            var stdout = process.StandardOutput.ReadToEndAsync();
            var stderr = process.StandardError.ReadToEndAsync();

            var completed = await Task.Run(() => process.WaitForExit(15000));
            if (!completed)
            {
                try { process.Kill(); } catch { }
                Log.Error("vgmstream conversion timed out");
                return null;
            }

            await Task.WhenAll(stdout, stderr);

            if (process.ExitCode != 0)
            {
                Log.Error("vgmstream exited with code {ExitCode}: {Stderr}", process.ExitCode, await stderr);
                return null;
            }

            if (!File.Exists(tempOutput))
            {
                Log.Error("vgmstream did not produce output file");
                return null;
            }

            var wavData = await File.ReadAllBytesAsync(tempOutput);
            Log.Information("Converted {Ext} to WAV via vgmstream ({InSize} -> {OutSize} bytes)",
                ext, audioData.Length, wavData.Length);
            return new ConversionResult(wavData, "wav");
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to convert {Ext} via vgmstream", ext);
            return null;
        }
        finally
        {
            SafeDelete(tempInput);
            SafeDelete(tempOutput);
        }
    }

    private static string? TryGetVgmstreamPath()
    {
        var appDir = AppContext.BaseDirectory;
        var candidates = new[]
        {
            Path.Combine(appDir, "vgmstream-cli.exe"),
            Path.Combine(appDir, ".data", "vgmstream-cli.exe"),
        };

        foreach (var candidate in candidates)
        {
            if (File.Exists(candidate))
            {
                Log.Debug("Found vgmstream at: {Path}", candidate);
                return candidate;
            }
        }

        try
        {
            var pathEnv = Environment.GetEnvironmentVariable("PATH");
            if (!string.IsNullOrEmpty(pathEnv))
            {
                foreach (var p in pathEnv.Split(Path.PathSeparator))
                {
                    var fullPath = Path.Combine(p, "vgmstream-cli.exe");
                    if (File.Exists(fullPath)) return fullPath;
                }
            }
        }
        catch { }

        return null;
    }

    private static int FindChunkOffset(byte[] riffData, string chunkId)
    {
        if (riffData.Length < 12) return -1;

        var offset = 12;
        while (offset + 8 <= riffData.Length)
        {
            var id = System.Text.Encoding.ASCII.GetString(riffData, offset, 4);
            var chunkSize = BitConverter.ToInt32(riffData, offset + 4);

            if (id == chunkId)
                return offset;

            offset += 8 + chunkSize;
            if (chunkSize % 2 != 0) offset++;
        }
        return -1;
    }

    private static Task<ConversionResult?> ConvertHcaAsync(byte[] audioData)
    {
        try
        {
            var wavData = HcaWaveStream.ConvertHcaToWav(audioData, 0);
            if (wavData == null || wavData.Length == 0)
            {
                Log.Warning("HCA decoder produced empty output");
                return Task.FromResult<ConversionResult?>(null);
            }

            Log.Information("Converted HCA to WAV ({InSize} -> {OutSize} bytes)",
                audioData.Length, wavData.Length);
            return Task.FromResult<ConversionResult?>(new ConversionResult(wavData, "wav"));
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to convert HCA to WAV");
            return Task.FromResult<ConversionResult?>(null);
        }
    }

    private static Task<ConversionResult?> ConvertAdxAsync(byte[] audioData)
    {
        try
        {
            var wavData = CUE4Parse.UE4.Criware.Decoders.ADX.AdxDecoder.ConvertAdxToWav(audioData, 0);
            if (wavData == null || wavData.Length == 0)
            {
                Log.Warning("ADX decoder produced empty output");
                return Task.FromResult<ConversionResult?>(null);
            }

            Log.Information("Converted ADX to WAV ({InSize} -> {OutSize} bytes)",
                audioData.Length, wavData.Length);
            return Task.FromResult<ConversionResult?>(new ConversionResult(wavData, "wav"));
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to convert ADX to WAV");
            return Task.FromResult<ConversionResult?>(null);
        }
    }

    private static void SafeDelete(string path)
    {
        try { if (File.Exists(path)) File.Delete(path); }
        catch { }
    }
}

public class ConversionResult
{
    public byte[] Data { get; }
    public string Format { get; }

    public ConversionResult(byte[] data, string format)
    {
        Data = data;
        Format = format;
    }
}
