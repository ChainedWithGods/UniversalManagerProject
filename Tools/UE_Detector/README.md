# UE Version Detector

A CLI tool to detect Unreal Engine version from game executables (.exe files).

## Features

- Detects UE version from PE file resources
- Supports UE 4.0 through UE 5.8+
- Does NOT scan pak/iostore files (only the .exe)
- Multiple output formats (plain text, JSON, verbose)

## Usage

```bash
UE_Detector [path_to_exe] [options]
```

**Note**: In PowerShell, use `.\UE_Detector.exe` when running from the current directory.

### Options
- `-h, --help` - Show help message
- `-v, --verbose` - Show detailed version information
- `-j, --json` - Output in JSON format

### Examples

#### PowerShell:
```powershell
# Show help
.\UE_Detector.exe --help

# Basic usage - outputs version string
.\UE_Detector.exe "C:\Games\MyGame\Binaries\Win64\MyGame.exe"
# Output: UE 4.27.2

# Auto-detect from current directory (place detector next to game .exe)
.\UE_Detector.exe
# Output: UE 4.27.2

# Verbose output
.\UE_Detector.exe "C:\Games\MyGame\Binaries\Win64\MyGame.exe" -v
# Output:
# UE Version: UE 4.27.2
# Major: 4
# Minor: 27
# Patch: 2
# Changelist: 0
# Branch: 
# Engine: Unreal Engine 4

# JSON output (useful for scripts)
.\UE_Detector.exe "C:\Games\MyGame\Binaries\Win64\MyGame.exe" -j
# Output: {"major":4,"minor":27,"patch":2,"changelist":0,"branch":"","is_ue5":false,"version_string":"UE 4.27.2"}
```

#### Command Prompt (CMD):
```cmd
REM In CMD, you don't need the .\ prefix
UE_Detector.exe "C:\Games\MyGame\Binaries\Win64\MyGame.exe"
UE_Detector.exe --help
UE_Detector.exe -v
```

## How It Works

The tool extracts version information from:
1. PE file version resources (ProductName, ProductVersion, FileVersion, FileDescription)
2. Binary pattern scanning for UE version strings
3. Exported module data

This approach only reads the .exe file and does NOT access pak or iostore files.

## Supported Versions

- Unreal Engine 4: 4.0 - 4.27.x
- Unreal Engine 5: 5.0.0 - 5.8.x