[CmdletBinding()]
param(
    [string]$BuildDir     = "",
    [string]$QtBinDir     = "",
    [string]$AppVersion   = "1.0.0",
    [string]$OutputDir    = "",
    [switch]$SkipWindeployqt,
    [switch]$DebugMode
)

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ScriptDir)) {
    $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
}

$RepoRoot     = (Resolve-Path -LiteralPath (Join-Path $ScriptDir "..")).Path
$ModsRoot     = $RepoRoot
$InstallerSrc = Join-Path $ScriptDir "src"
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $ScriptDir "output"
}

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot "build\Desktop_Qt_6_12_0_MSVC2022_64bit_Release"
}

if (-not (Test-Path -LiteralPath $BuildDir)) {
    throw "Build directory not found: $BuildDir`nPass -BuildDir to specify the Qt cmake build output directory."
}

function Ensure-Dir([string]$p) {
    if (-not (Test-Path -LiteralPath $p)) { New-Item -ItemType Directory -Path $p | Out-Null }
}

function Copy-Safe([string]$src, [string]$dst) {
    if (Test-Path -LiteralPath $src) {
        Ensure-Dir (Split-Path -Parent $dst)
        Copy-Item -LiteralPath $src -Destination $dst -Force
    } else {
        Write-Warning "Skipping missing file: $src"
    }
}

function Copy-Optional([string]$src, [string]$dst) {
    if (Test-Path -LiteralPath $src) {
        Ensure-Dir (Split-Path -Parent $dst)
        Copy-Item -LiteralPath $src -Destination $dst -Force
    }
}

function Copy-Dir([string]$src, [string]$dst, [string[]]$Exclude = @()) {
    if (-not (Test-Path -LiteralPath $src)) { Write-Warning "Skipping missing folder: $src"; return }
    Ensure-Dir $dst
    Get-ChildItem -LiteralPath $src -Force | ForEach-Object {
        $item = $_
        $skip = $false
        foreach ($pat in $Exclude) {
            if ($item.Name -like $pat) { $skip = $true; break }
        }
        if (-not $skip) {
            Copy-Item -LiteralPath $item.FullName -Destination $dst -Recurse -Force
        }
    }
}

function Resolve-Tool([string]$exe, [string]$pref = "") {
    if ($pref) {
        $c = Join-Path $pref $exe
        if (Test-Path -LiteralPath $c) { return (Resolve-Path -LiteralPath $c).Path }
    }
    $cmd = Get-Command $exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

$windeployqt = $null
if (-not $SkipWindeployqt) {
    $qtDirBin = if ($env:QTDIR) { Join-Path $env:QTDIR "bin" } else { $null }
    $qtCandidates = @(
        $QtBinDir,
        $qtDirBin,
        "C:\Qt\6.12.0\msvc2022_64\bin"
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }

    foreach ($cand in $qtCandidates) {
        $windeployqt = Resolve-Tool "windeployqt.exe" $cand
        if ($windeployqt) { break }
    }
    if (-not $windeployqt) {
        $windeployqt = Resolve-Tool "windeployqt.exe"
    }

    if ($windeployqt) {
        Write-Host "Found windeployqt: $windeployqt" -ForegroundColor Green
    } else {
        Write-Warning "windeployqt.exe not found. Staging will copy pre-deployed Qt DLLs directly from build directory."
    }
}

$sevenZip = Resolve-Tool "7z.exe" "C:\Program Files\7-Zip"
if (-not $sevenZip) { $sevenZip = Resolve-Tool "7z.exe" "C:\Program Files (x86)\7-Zip" }
if (-not $sevenZip) { $sevenZip = Resolve-Tool "7z.exe" "C:\Program Files (x86)\Lua\5.1" }
if ($sevenZip) {
    Write-Host "Found 7-Zip: $sevenZip (high-ratio LZMA2 solid compression enabled)" -ForegroundColor Green
} else {
    Write-Warning "7z.exe not found. Solid LZMA2 compression disabled; will fall back to ZIP."
}

Write-Host ""
Write-Host "=== Step 1: Locating Pre-built Uninstaller ===" -ForegroundColor Cyan

$CmakeOutDir    = Join-Path $ScriptDir "build\cmake_out"
$UninstallerExe = Join-Path $CmakeOutDir "uninstall.exe"

if (-not (Test-Path -LiteralPath $UninstallerExe)) {
    throw "Pre-compiled uninstaller not found: $UninstallerExe`nPlease build the 'uninstall' project in Visual Studio first."
}

$uninstSize = (Get-Item -LiteralPath $UninstallerExe).Length
Write-Host "Found uninstaller: $UninstallerExe ($([Math]::Round($uninstSize / 1KB, 1)) KB)" -ForegroundColor Green

Write-Host ""
Write-Host "=== Step 2: Staging Payload ===" -ForegroundColor Cyan

$StagingDir = Join-Path $ScriptDir "build\staging"
if (Test-Path -LiteralPath $StagingDir) { Remove-Item -LiteralPath $StagingDir -Recurse -Force }
Ensure-Dir $StagingDir

$AppExe = Join-Path $BuildDir "UniversalManager.exe"
if (-not (Test-Path -LiteralPath $AppExe)) {
    throw "Application executable not found: $AppExe"
}
Copy-Safe $AppExe (Join-Path $StagingDir "UniversalManager.exe")

Write-Host "Staging Qt DLLs and plugins from build directory: $BuildDir" -ForegroundColor Yellow
Get-ChildItem -LiteralPath $BuildDir -Filter "*.dll" -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $StagingDir -Force
}

$qtPluginDirs = @("platforms", "qml", "styles", "imageformats", "multimedia", "generic", "iconengines", "networkinformation", "platforminputcontexts", "tls", "vectorimageformats")
foreach ($dir in $qtPluginDirs) {
    $srcDir = Join-Path $BuildDir $dir
    if (Test-Path -LiteralPath $srcDir) {
        Write-Host "Staging Qt directory: $dir" -ForegroundColor DarkGray
        & robocopy $srcDir (Join-Path $StagingDir $dir) /E /COPY:DAT /R:1 /W:1 /NP /NFL /NDL | Out-Null
    }
}

if ($windeployqt -and -not $SkipWindeployqt) {
    Write-Host "Running windeployqt to verify complete deployment..." -ForegroundColor Yellow
    $deployMode = if ($DebugMode) { "--debug" } else { "--release" }
    & $windeployqt `
        $deployMode `
        --no-compiler-runtime `
        --no-opengl-sw `
        --no-system-dxc-compiler `
        --no-translations `
        --qmldir (Join-Path $ModsRoot "qml") `
        --dir $StagingDir `
        (Join-Path $StagingDir "UniversalManager.exe")

    @("vc_redist.x64.exe", "opengl32sw.dll") | ForEach-Object {
        $stray = Join-Path $StagingDir $_
        if (Test-Path -LiteralPath $stray) {
            Remove-Item -LiteralPath $stray -Force -ErrorAction SilentlyContinue
        }
    }
}

$AssetsSource = if (Test-Path -LiteralPath (Join-Path $BuildDir "assets")) {
    Join-Path $BuildDir "assets"
} else {
    Join-Path $ModsRoot "assets"
}
Ensure-Dir (Join-Path $StagingDir "assets")
& robocopy $AssetsSource (Join-Path $StagingDir "assets") /E /COPY:DAT /R:1 /W:1 /NP /NFL /NDL | Out-Null

$DocsRoot = Join-Path $ModsRoot "docs"
Copy-Optional (Join-Path $DocsRoot "Readme.md")               (Join-Path $StagingDir "Readme.md")
Copy-Optional (Join-Path $DocsRoot "Requirements.md")         (Join-Path $StagingDir "Requirements.md")
Copy-Safe     (Join-Path $ModsRoot "LICENSE")                 (Join-Path $StagingDir "LICENSE")
Copy-Optional (Join-Path $DocsRoot "THIRD-PARTY-LICENSES.md") (Join-Path $StagingDir "THIRD-PARTY-LICENSES.md")

$ToolsBase = Join-Path $ModsRoot "Tools"

Write-Host "Staging pre-compressed tool archives..." -ForegroundColor Yellow

$pakViewer7z = Join-Path $ToolsBase "PakViewer\bin\Release\net10.0-windows10.0.19041.0\PakViewer.7z"
if (Test-Path -LiteralPath $pakViewer7z) {
    Ensure-Dir (Join-Path $StagingDir "Tools")
    Copy-Safe $pakViewer7z (Join-Path $StagingDir "Tools\PakViewer.7z")
    Write-Host "  + PakViewer.7z: $([Math]::Round((Get-Item -LiteralPath $pakViewer7z).Length / 1MB, 2)) MB" -ForegroundColor DarkGray
} else {
    Write-Warning "PakViewer.7z not found at: $pakViewer7z"
}

$mcd7z = Join-Path $ToolsBase "ModConflictDetector\bin\Release\ModConflictDetector.7z"
if (Test-Path -LiteralPath $mcd7z) {
    Copy-Safe $mcd7z (Join-Path $StagingDir "Tools\ModConflictDetector.7z")
    Write-Host "  + ModConflictDetector.7z: $([Math]::Round((Get-Item -LiteralPath $mcd7z).Length / 1MB, 2)) MB" -ForegroundColor DarkGray
} else {
    Write-Warning "ModConflictDetector.7z not found at: $mcd7z"
}

$avaloniumDll7z = Join-Path $ToolsBase "Avalonium\Runtime\DLL\DLL.7z"
if (Test-Path -LiteralPath $avaloniumDll7z) {
    Ensure-Dir (Join-Path $StagingDir "Tools\Avalonium\Runtime")
    Copy-Safe $avaloniumDll7z (Join-Path $StagingDir "Tools\Avalonium\Runtime\DLL.7z")
    Write-Host "  + Avalonium DLL.7z: $([Math]::Round((Get-Item -LiteralPath $avaloniumDll7z).Length / 1MB, 2)) MB" -ForegroundColor DarkGray
} else {
    Write-Warning "Avalonium DLL.7z not found at: $avaloniumDll7z"
}

Copy-Safe `
    (Join-Path $ToolsBase "UE_Detector\Idk\UE_Detector.exe") `
    (Join-Path $StagingDir "Tools\UE_Detector\UE_Detector.exe")

Copy-Dir  (Join-Path $ToolsBase "Avalonium\Helpers\Licenses") (Join-Path $StagingDir "Tools\Avalonium\Helpers\Licenses")
Copy-Safe (Join-Path $ToolsBase "Avalonium\Helpers\injector.py")           (Join-Path $StagingDir "Tools\Avalonium\Helpers\injector.py")
Copy-Safe (Join-Path $ToolsBase "Avalonium\Helpers\iocp_client.py")        (Join-Path $StagingDir "Tools\Avalonium\Helpers\iocp_client.py")
Copy-Safe (Join-Path $ToolsBase "Avalonium\Helpers\SDKOrchestrator.py")    (Join-Path $StagingDir "Tools\Avalonium\Helpers\SDKOrchestrator.py")
Copy-Safe (Join-Path $ToolsBase "Avalonium\Helpers\MappingOrchestrator.py")(Join-Path $StagingDir "Tools\Avalonium\Helpers\MappingOrchestrator.py")
Copy-Safe (Join-Path $ToolsBase "Avalonium\Helpers\GenerateBuild.py")      (Join-Path $StagingDir "Tools\Avalonium\Helpers\GenerateBuild.py")

Copy-Safe $UninstallerExe (Join-Path $StagingDir "uninstall.exe")

Write-Host "Staging complete: $StagingDir" -ForegroundColor Green

Write-Host ""
Write-Host "=== Step 3: Compressing Payload ===" -ForegroundColor Cyan

$Archive7z  = Join-Path $ScriptDir "build\staging.7z"
$ArchiveZip = Join-Path $ScriptDir "build\staging.zip"

if (Test-Path -LiteralPath $Archive7z)  { Remove-Item -LiteralPath $Archive7z -Force }
if (Test-Path -LiteralPath $ArchiveZip) { Remove-Item -LiteralPath $ArchiveZip -Force }

if ($sevenZip) {
    Write-Host "Creating app.7z using 7-Zip solid LZMA2 ultra compression (multi-threaded)..." -ForegroundColor Yellow
    & $sevenZip a -t7z -m0=lzma2:d128m:fb273 -ms=on -mmt=on -myx=9 -mqs=on -bd $Archive7z "$StagingDir\*"

    if ($LASTEXITCODE -ne 0) { throw "7-Zip compression failed with exit code $LASTEXITCODE." }

    $archiveSize = (Get-Item -LiteralPath $Archive7z).Length
    Write-Host "app.7z created: $Archive7z ($([Math]::Round($archiveSize / 1MB, 1)) MB)" -ForegroundColor Green
} else {
    Write-Host "7-Zip not found, falling back to standard ZipFile..." -ForegroundColor Yellow
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::CreateFromDirectory(
        $StagingDir,
        $ArchiveZip,
        [System.IO.Compression.CompressionLevel]::Optimal,
        $false
    )

    $archiveSize = (Get-Item -LiteralPath $ArchiveZip).Length
    Write-Host "app.zip created: $ArchiveZip ($([Math]::Round($archiveSize / 1MB, 1)) MB)" -ForegroundColor Green
}

Write-Host ""
Write-Host "=== Step 4: Appending Overlay Payload to Installer PE ===" -ForegroundColor Cyan

Ensure-Dir $OutputDir
Get-ChildItem $OutputDir -Filter "*.pdb" -ErrorAction SilentlyContinue | Remove-Item -Force

if (-not (Test-Path -LiteralPath $Archive7z)) {
    throw "Payload archive not found: $Archive7z"
}

$PeOnly = Join-Path $CmakeOutDir "UniversalModManager-Setup.exe"
if (-not (Test-Path -LiteralPath $PeOnly)) {
    throw "Pre-compiled installer PE stub not found: $PeOnly`nPlease build the 'UniversalModManager-Setup' project in Visual Studio first."
}

$peSize = [Math]::Round((Get-Item -LiteralPath $PeOnly).Length / 1KB, 1)
Write-Host "Found PE stub: $peSize KB" -ForegroundColor Green

Write-Host "Appending staging.7z overlay to PE..." -ForegroundColor Yellow

$FinalExeName = "UniversalModManager-Setup-$AppVersion.exe"
$FinalExeDest = Join-Path $OutputDir $FinalExeName
if (Test-Path -LiteralPath $FinalExeDest) { Remove-Item -LiteralPath $FinalExeDest -Force }

Copy-Item -LiteralPath $PeOnly -Destination $FinalExeDest -Force

$archiveBytes = (Get-Item -LiteralPath $Archive7z).Length
$chunkSize    = 4MB
$fsOut = [System.IO.File]::Open($FinalExeDest, 'Append', 'Write', 'None')
$fsIn  = [System.IO.File]::OpenRead($Archive7z)
$buf   = [byte[]]::new($chunkSize)
$totalWritten = 0L
while ($true) {
    $read = $fsIn.Read($buf, 0, $buf.Length)
    if ($read -le 0) { break }
    $fsOut.Write($buf, 0, $read)
    $totalWritten += $read
}
$fsIn.Close()

$OVERLAY_MAGIC = 0x524D4D55564E4957L
$footer = [byte[]]::new(16)
[System.BitConverter]::GetBytes([uint64]$totalWritten).CopyTo($footer, 0)
[System.BitConverter]::GetBytes([uint64]$OVERLAY_MAGIC).CopyTo($footer, 8)
$fsOut.Write($footer, 0, 16)
$fsOut.Close()

$setupSize = [Math]::Round((Get-Item -LiteralPath $FinalExeDest).Length / 1MB, 2)
$archiveMB = [Math]::Round($archiveBytes / 1MB, 1)
Write-Host "Overlay appended: $archiveMB MB payload + 16 byte footer" -ForegroundColor Green
Write-Host "Final installer : $FinalExeDest ($setupSize MB)" -ForegroundColor Green

Write-Host ""
Write-Host "===================================================================" -ForegroundColor Cyan
Write-Host "  Build complete!)" -ForegroundColor Green
Write-Host ""
Write-Host "  PE stub       : $peSize KB"
Write-Host "  Payload       : $archiveMB MB"
Write-Host "  Installer     : $FinalExeDest ($setupSize MB)"
Write-Host "===================================================================" -ForegroundColor Cyan
Write-Host ""
