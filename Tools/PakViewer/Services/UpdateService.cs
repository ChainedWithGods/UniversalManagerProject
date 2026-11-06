using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Newtonsoft.Json;
using Serilog;
using PakViewer.Settings;

namespace PakViewer.Services;

public class UpdateManifest
{
    [JsonProperty("version")]
    public string Version { get; set; } = string.Empty;

    [JsonProperty("changelog")]
    public string Changelog { get; set; } = string.Empty;

    [JsonProperty("downloadUrl")]
    public string DownloadUrl { get; set; } = string.Empty;

    [JsonProperty("isMandatory")]
    public bool IsMandatory { get; set; } = false;

    [JsonProperty("releaseDate")]
    public string? ReleaseDate { get; set; }
}

public class UpdateCheckResult
{
    public bool HasUpdate { get; set; }
    public Version? CurrentVersion { get; set; }
    public Version? LatestVersion { get; set; }
    public UpdateManifest? Manifest { get; set; }
    public string? ErrorMessage { get; set; }
}

public class UpdateDownloadProgress
{
    public long BytesDownloaded { get; set; }
    public long? TotalBytes { get; set; }
    public double ProgressPercentage => TotalBytes.HasValue && TotalBytes.Value > 0
        ? (double)BytesDownloaded / TotalBytes.Value * 100.0
        : 0.0;
    public string StatusMessage { get; set; } = string.Empty;
}

public class UpdateService
{

    public const string DefaultManifestUrl = "https://pakviewer.netlify.app/version.json";

    private static readonly Lazy<HttpClient> SharedClient = new(() =>
    {
        var handler = new HttpClientHandler
        {
            AllowAutoRedirect = true,
            AutomaticDecompression = DecompressionMethods.GZip | DecompressionMethods.Deflate | DecompressionMethods.Brotli,
            CookieContainer = new CookieContainer()
        };

        var client = new HttpClient(handler)
        {
            Timeout = TimeSpan.FromSeconds(60)
        };
        client.DefaultRequestHeaders.Add("User-Agent", "PakViewer-Updater/1.0");
        return client;
    });

    private static HttpClient Client => SharedClient.Value;

    public static Version GetCurrentVersion()
    {
        try
        {

            var assembly = Assembly.GetEntryAssembly() ?? Assembly.GetExecutingAssembly();
            var assemblyVersion = assembly.GetName().Version;

            if (assemblyVersion != null)
            {
                Log.Information("Detected assembly version: {Version}", assemblyVersion);
                return assemblyVersion;
            }

            var exePath = Environment.ProcessPath ?? Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "PakViewer.exe");
            if (File.Exists(exePath))
            {
                var fileVersionInfo = FileVersionInfo.GetVersionInfo(exePath);

                if (fileVersionInfo.FileMajorPart > 0 || fileVersionInfo.FileMinorPart > 0 || fileVersionInfo.FileBuildPart > 0)
                {
                    var fileVersion = new Version(
                        fileVersionInfo.FileMajorPart,
                        fileVersionInfo.FileMinorPart,
                        Math.Max(0, fileVersionInfo.FileBuildPart),
                        Math.Max(0, fileVersionInfo.FilePrivatePart)
                    );
                    Log.Debug("Detected file version: {Version}", fileVersion);
                    return fileVersion;
                }

                if (fileVersionInfo.ProductMajorPart > 0 || fileVersionInfo.ProductMinorPart > 0 || fileVersionInfo.ProductBuildPart > 0)
                {
                    var productVersion = new Version(
                        fileVersionInfo.ProductMajorPart,
                        fileVersionInfo.ProductMinorPart,
                        Math.Max(0, fileVersionInfo.ProductBuildPart),
                        Math.Max(0, fileVersionInfo.ProductPrivatePart)
                    );
                    Log.Debug("Detected product version: {Version}", productVersion);
                    return productVersion;
                }
            }
        }
        catch (Exception ex)
        {
            Log.Warning(ex, "Failed to detect application version, falling back to default");
        }

        var defaultVersion = new Version(1, 0, 0, 0);
        Log.Warning("Could not detect application version, using default: {Version}", defaultVersion);
        return defaultVersion;
    }

    public static string NormalizeDownloadUrl(string url, bool isJsonManifest)
    {
        if (string.IsNullOrWhiteSpace(url))
            return string.Empty;

        var clean = url.Trim();
        if (clean.Contains("dropbox.com", StringComparison.OrdinalIgnoreCase))
        {
            var targetParam = isJsonManifest ? "raw=1" : "dl=1";
            clean = Regex.Replace(clean, @"[?&](dl=[01]|raw=1)", string.Empty, RegexOptions.IgnoreCase);
            return clean.Contains('?') ? $"{clean}&{targetParam}" : $"{clean}?{targetParam}";
        }

        return clean;
    }

    public async Task<UpdateCheckResult> CheckForUpdatesAsync(string? manifestUrl = null, CancellationToken ct = default)
    {
        var result = new UpdateCheckResult
        {
            CurrentVersion = GetCurrentVersion()
        };

        var targetUrl = string.IsNullOrWhiteSpace(manifestUrl) ? DefaultManifestUrl : manifestUrl;

        if (string.IsNullOrWhiteSpace(targetUrl) || targetUrl.Contains("YOUR_LINK_HERE", StringComparison.OrdinalIgnoreCase))
        {
            result.ErrorMessage = "No update server configured.";
            return result;
        }

        if (targetUrl.Contains(".7z", StringComparison.OrdinalIgnoreCase) ||
            targetUrl.Contains(".zip", StringComparison.OrdinalIgnoreCase) ||
            targetUrl.Contains(".rar", StringComparison.OrdinalIgnoreCase))
        {
            result.ErrorMessage = "Invalid update manifest configuration.";
            Log.Warning("Update check skipped: Manifest URL points to an archive file instead of metadata.");
            return result;
        }

        try
        {
            var directUrl = NormalizeDownloadUrl(targetUrl, isJsonManifest: true);
            Log.Information("Checking for updates...");

            using var response = await Client.GetAsync(directUrl, HttpCompletionOption.ResponseContentRead, ct);
            response.EnsureSuccessStatusCode();

            var jsonContent = await response.Content.ReadAsStringAsync(ct);
            if (string.IsNullOrWhiteSpace(jsonContent))
            {
                result.ErrorMessage = "Empty response received from update server.";
                return result;
            }

            var manifest = JsonConvert.DeserializeObject<UpdateManifest>(jsonContent);
            if (manifest == null || string.IsNullOrWhiteSpace(manifest.Version))
            {
                result.ErrorMessage = "Invalid update manifest format.";
                return result;
            }

            result.Manifest = manifest;

            var cleanVersionStr = manifest.Version.Trim().TrimStart('v', 'V');
            if (Version.TryParse(cleanVersionStr, out var latestVer))
            {
                result.LatestVersion = latestVer;
                result.HasUpdate = latestVer > result.CurrentVersion;
            }
            else
            {
                Log.Warning("Could not parse version string: {VersionString}", manifest.Version);
                result.ErrorMessage = $"Failed to parse version: {manifest.Version}";
            }

            Log.Information("Update check complete. Current: {Current}, Latest: {Latest}, HasUpdate: {HasUpdate}",
                result.CurrentVersion, result.LatestVersion, result.HasUpdate);

            return result;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to check for updates");
            result.ErrorMessage = ex.Message;
            return result;
        }
    }

    public async Task<string> DownloadUpdateAsync(
        string downloadUrl,
        IProgress<UpdateDownloadProgress>? progress = null,
        CancellationToken ct = default)
    {
        var directUrl = NormalizeDownloadUrl(downloadUrl, isJsonManifest: false);
        Log.Information("Starting update package download...");

        var tempDir = Path.Combine(Path.GetTempPath(), "PakViewer", "Updates");
        Directory.CreateDirectory(tempDir);
        var ext = directUrl.Contains(".7z", StringComparison.OrdinalIgnoreCase) ? ".7z" : ".zip";
        var targetZipPath = Path.Combine(tempDir, $"PakViewer_Update{ext}");

        if (File.Exists(targetZipPath))
        {
            try { File.Delete(targetZipPath); } catch { }
        }

        using var response = await Client.GetAsync(directUrl, HttpCompletionOption.ResponseHeadersRead, ct);
        response.EnsureSuccessStatusCode();

        var totalBytes = response.Content.Headers.ContentLength;

        await using var contentStream = await response.Content.ReadAsStreamAsync(ct);
        await using var fileStream = new FileStream(targetZipPath, FileMode.Create, FileAccess.Write, FileShare.None, 81920, true);

        var buffer = new byte[81920];
        long totalRead = 0;
        int bytesRead;

        while ((bytesRead = await contentStream.ReadAsync(buffer, 0, buffer.Length, ct)) > 0)
        {
            await fileStream.WriteAsync(buffer.AsMemory(0, bytesRead), ct);
            totalRead += bytesRead;

            progress?.Report(new UpdateDownloadProgress
            {
                BytesDownloaded = totalRead,
                TotalBytes = totalBytes,
                StatusMessage = "Downloading update..."
            });
        }

        progress?.Report(new UpdateDownloadProgress
        {
            BytesDownloaded = totalRead,
            TotalBytes = totalBytes ?? totalRead,
            StatusMessage = "Download complete!"
        });

        Log.Information("Update downloaded successfully to: {Path} ({Bytes} bytes)", targetZipPath, totalRead);
        return targetZipPath;
    }

    public static void ApplyUpdateAndRestart(string zipFilePath, string? newVersion = null)
    {
        try
        {
            var currentProcess = Process.GetCurrentProcess();
            var currentPid = currentProcess.Id;
            var appDir = AppDomain.CurrentDomain.BaseDirectory.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            var exePath = Environment.ProcessPath ?? Path.Combine(appDir, "PakViewer.exe");

            var tempDir = Path.Combine(Path.GetTempPath(), "PakViewer", "Updates");
            Directory.CreateDirectory(tempDir);
            var scriptPath = Path.Combine(tempDir, "apply_update.cmd");
            var logPath = Path.Combine(tempDir, "update_log.txt");

            var scriptContent =
$@"@echo off
setlocal enabledelayedexpansion
title PakViewer Auto-Updater

set ""TARGET_DIR={appDir}""
set ""ZIP_FILE={zipFilePath}""
set ""TARGET_PID={currentPid}""
set ""EXE_PATH={exePath}""
set ""LOG_FILE={logPath}""

echo ====================================== >> ""%LOG_FILE%""
echo PakViewer Auto-Updater Started >> ""%LOG_FILE%""
echo Time: %date% %time% >> ""%LOG_FILE%""
echo Target Directory: %TARGET_DIR% >> ""%LOG_FILE%""
echo ZIP File: %ZIP_FILE% >> ""%LOG_FILE%""
echo Target PID: %TARGET_PID% >> ""%LOG_FILE%""
echo EXE Path: %EXE_PATH% >> ""%LOG_FILE%""
echo ====================================== >> ""%LOG_FILE%""

echo Waiting for PakViewer (PID: %TARGET_PID%) to exit... >> ""%LOG_FILE%""
set /a WAIT_COUNT=0
:WAIT_LOOP
tasklist /FI ""PID eq %TARGET_PID%"" 2>NUL | find /I ""%TARGET_PID%"" >NUL
if not errorlevel 1 (
    set /a WAIT_COUNT+=1
    if !WAIT_COUNT! GEQ 10 (
        echo Force killing PakViewer process PID %TARGET_PID%... >> ""%LOG_FILE%""
        taskkill /F /PID %TARGET_PID% 2>NUL
    )
    timeout /t 1 /nobreak >NUL
    goto WAIT_LOOP
)

echo Process exited, waiting for file locks to release... >> ""%LOG_FILE%""
timeout /t 2 /nobreak >NUL

echo Starting extraction... >> ""%LOG_FILE%""

set ""EXTRACTED=0""
set ""SEVENZIP=""

:: Search for 7-Zip executable in known paths
if exist ""%ProgramFiles%\7-Zip\7z.exe"" set ""SEVENZIP=%ProgramFiles%\7-Zip\7z.exe""
if not defined SEVENZIP if exist ""%ProgramFiles(x86)%\7-Zip\7z.exe"" set ""SEVENZIP=%ProgramFiles(x86)%\7-Zip\7z.exe""
if not defined SEVENZIP if exist ""%TARGET_DIR%\..\..\7z.exe"" set ""SEVENZIP=%TARGET_DIR%\..\..\7z.exe""
if not defined SEVENZIP if exist ""%TARGET_DIR%\..\7z.exe"" set ""SEVENZIP=%TARGET_DIR%\..\7z.exe""
if not defined SEVENZIP if exist ""%TARGET_DIR%\7z.exe"" set ""SEVENZIP=%TARGET_DIR%\7z.exe""

if not defined SEVENZIP (
    for /f ""tokens=* delims="" %%I in ('where 7z.exe 2^>NUL') do (
        if exist ""%%I"" set ""SEVENZIP=%%I""
    )
)

:: Method 1: Use 7-Zip if available (handles both .7z and .zip)
if defined SEVENZIP (
    echo Using 7-Zip at ""!SEVENZIP!""... >> ""%LOG_FILE%""
    ""!SEVENZIP!"" x -y -o""%TARGET_DIR%"" ""%ZIP_FILE%"" >> ""%LOG_FILE%"" 2>&1
    if not errorlevel 1 set ""EXTRACTED=1""
)

:: Method 2: Use Windows tar (Windows 10/11)
if ""!EXTRACTED!""==""0"" (
    echo Trying Windows tar extraction... >> ""%LOG_FILE%""
    tar -xf ""%ZIP_FILE%"" -C ""%TARGET_DIR%"" >> ""%LOG_FILE%"" 2>&1
    if not errorlevel 1 set ""EXTRACTED=1""
)

:: Method 3: PowerShell Expand-Archive (for .zip files)
if ""!EXTRACTED!""==""0"" (
    echo Trying PowerShell Expand-Archive... >> ""%LOG_FILE%""
    powershell -NoProfile -ExecutionPolicy Bypass -Command ""try {{ Expand-Archive -LiteralPath '%ZIP_FILE%' -DestinationPath '%TARGET_DIR%' -Force; exit 0 }} catch {{ Write-Error $_.Exception.Message; exit 1 }}"" >> ""%LOG_FILE%"" 2>&1
    if not errorlevel 1 set ""EXTRACTED=1""
)

if ""!EXTRACTED!""==""0"" (
    echo ERROR: All extraction methods failed! >> ""%LOG_FILE%""
    exit /b 1
)

echo Extraction completed successfully >> ""%LOG_FILE%""

:: If archive was packed as a subfolder (e.g. PakViewer\ or publish\), flatten it
echo Checking for nested folders... >> ""%LOG_FILE%""
for /d %%D in (""%TARGET_DIR%\*"") do (
    if exist ""%%D\PakViewer.exe"" (
        echo Found nested PakViewer.exe in %%D, flattening... >> ""%LOG_FILE%""
        xcopy ""%%D\*"" ""%TARGET_DIR%\"" /E /Y /H /Q >> ""%LOG_FILE%"" 2>&1
        rd /s /q ""%%D"" 2>> ""%LOG_FILE%""
    )
)

:: Verify the exe exists after extraction
if not exist ""%EXE_PATH%"" (
    echo ERROR: PakViewer.exe not found after extraction at: %EXE_PATH% >> ""%LOG_FILE%""
    exit /b 1
)

echo Cleaning up update package... >> ""%LOG_FILE%""
del /f /q ""%ZIP_FILE%"" 2>NUL

echo Launching updated PakViewer from: %EXE_PATH% >> ""%LOG_FILE%""
start """" ""%EXE_PATH%""

echo Update completed successfully! >> ""%LOG_FILE%""

:: Clean up updater script itself
timeout /t 1 /nobreak >NUL
(goto) 2>nul & del ""%~f0""
";
            File.WriteAllText(scriptPath, scriptContent);

            Log.Information("Launching updater script: {ScriptPath}", scriptPath);
            Log.Information("Update log will be written to: {LogPath}", logPath);

            var psi = new ProcessStartInfo
            {
                FileName = "cmd.exe",
                Arguments = $"/c \"{scriptPath}\"",
                UseShellExecute = true,
                Verb = "runas",
                WindowStyle = ProcessWindowStyle.Hidden
            };

            Process.Start(psi);

            Environment.Exit(0);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to apply update and restart");
            throw;
        }
    }
}
