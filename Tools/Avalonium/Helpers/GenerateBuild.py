#!/usr/bin/env python3
"""
GenerateBuild.py - One-click build-system generator for UE4/UE5 SDK dumps.

Run this after SDK generation to produce:
  - CMakeLists.txt
  - <Game>_SDK.vcxproj
  - <Game>_SDK.vcxproj.filters
  - <Game>_SDK.sln
  - compile_commands.json

Usage:
    python GenerateBuild.py [sdk_root] [--game-name <name>]
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import uuid
import xml.etree.ElementTree as ET
from pathlib import Path


VSWHERE_CANDIDATES = (
    Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe",
    Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe",
)

WINDOWS_KITS_INCLUDE_ROOT = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Windows Kits" / "10" / "Include"


def generate_guid() -> str:
    return "{" + str(uuid.uuid4()).upper() + "}"


def scan_sdk_files(sdk_root: Path):
    """Scan the SDK folder for .cpp and .h files."""
    cpp_files = sorted([f.name for f in sdk_root.glob("*.cpp")])
    h_files = sorted([f.name for f in sdk_root.glob("*.h")])
    return cpp_files, h_files


def sanitize_identifier(name: str) -> str:
    """Sanitize a game name for use as a CMake/VS project identifier."""
    result = re.sub(r"[^A-Za-z0-9_]+", "_", name)
    result = re.sub(r"_+", "_", result)
    result = result.strip("_")
    if result and result[0].isdigit():
        result = "_" + result
    return result if result else "UE_SDK"


def write_cmake(sdk_root: Path, game_name: str, cpp_files: list[str]):
    cmake_path = sdk_root / "CMakeLists.txt"
    safe_name = sanitize_identifier(game_name) if game_name else "UE_SDK"
    project_name = f"{safe_name}_SDK" if game_name else "UE_SDK"
    file_list = "\n".join([f'    "${{CMAKE_CURRENT_SOURCE_DIR}}/{f}"' for f in cpp_files])

    content = f"""cmake_minimum_required(VERSION 3.15)
project({project_name} LANGUAGES CXX)

# Increase max object path limit to prevent warnings on Windows
set(CMAKE_OBJECT_PATH_MAX 500)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SDK_SOURCES
{file_list}
)

add_library(SDK STATIC ${{SDK_SOURCES}})

target_include_directories(SDK PUBLIC
    ${{CMAKE_CURRENT_SOURCE_DIR}}
)

if(MSVC)
    target_compile_definitions(SDK PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(SDK PRIVATE /W3 /permissive-)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
foreach(source ${{SDK_SOURCES}})
    get_filename_component(fname ${{source}} NAME_WE)
    string(REGEX REPLACE "_.*$" "" pkg_name ${{fname}})
    source_group(\\"${{pkg_name}}\\" FILES ${{source}})
endforeach()

message(STATUS "{project_name} - {len(cpp_files)} source files configured.")
"""

    cmake_path.write_text(content, encoding="utf-8")
    print(f"[+] CMakeLists.txt   -> {cmake_path}")


def write_vcxproj(sdk_root: Path, game_name: str, cpp_files: list[str], h_files: list[str]):
    safe_name = sanitize_identifier(game_name) if game_name else "UE_SDK"
    proj_name = f"{safe_name}_SDK" if game_name else "UE_SDK"
    proj_guid = generate_guid()
    vcxproj_path = sdk_root / f"{proj_name}.vcxproj"

    root = ET.Element("Project")
    root.set("xmlns", "http://schemas.microsoft.com/developer/msbuild/2003")
    root.set("DefaultTargets", "Build")

    configs = ET.SubElement(root, "ItemGroup", {"Label": "ProjectConfigurations"})
    for cfg, plat in [("Debug", "Win32"), ("Release", "Win32"), ("Debug", "x64"), ("Release", "x64")]:
        pc = ET.SubElement(configs, "ProjectConfiguration", {"Include": f"{cfg}|{plat}"})
        ET.SubElement(pc, "Configuration").text = cfg
        ET.SubElement(pc, "Platform").text = plat

    globals_pg = ET.SubElement(root, "PropertyGroup", {"Label": "Globals"})
    ET.SubElement(globals_pg, "VCProjectVersion").text = "16.0"
    ET.SubElement(globals_pg, "ProjectGuid").text = proj_guid
    ET.SubElement(globals_pg, "RootNamespace").text = proj_name
    ET.SubElement(globals_pg, "WindowsTargetPlatformVersion").text = "10.0"

    ET.SubElement(root, "Import", {"Project": "$(VCTargetsPath)\\Microsoft.Cpp.Default.props"})

    for cfg, plat in [("Debug", "Win32"), ("Release", "Win32"), ("Debug", "x64"), ("Release", "x64")]:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        pg = ET.SubElement(root, "PropertyGroup", {"Condition": cond, "Label": "Configuration"})
        ET.SubElement(pg, "ConfigurationType").text = "StaticLibrary"
        ET.SubElement(pg, "UseDebugLibraries").text = "true" if cfg == "Debug" else "false"
        ET.SubElement(pg, "PlatformToolset").text = "v143"
        ET.SubElement(pg, "CharacterSet").text = "Unicode"
        if cfg == "Release":
            ET.SubElement(pg, "WholeProgramOptimization").text = "true"

    ET.SubElement(root, "Import", {"Project": "$(VCTargetsPath)\\Microsoft.Cpp.props"})
    for cfg, plat in [("Debug", "Win32"), ("Release", "Win32"), ("Debug", "x64"), ("Release", "x64")]:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        ig = ET.SubElement(root, "ImportGroup", {"Label": "PropertySheets", "Condition": cond})
        ET.SubElement(
            ig,
            "Import",
            {
                "Project": "$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props",
                "Condition": "exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')",
                "Label": "LocalAppDataPlatform",
            },
        )

    ET.SubElement(root, "PropertyGroup", {"Label": "UserMacros"})

    for cfg, plat in [("Debug", "Win32"), ("Release", "Win32"), ("Debug", "x64"), ("Release", "x64")]:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        pg = ET.SubElement(root, "PropertyGroup", {"Condition": cond})
        ET.SubElement(pg, "OutDir").text = "$(SolutionDir)bin\\$(Platform)\\$(Configuration)\\"
        ET.SubElement(pg, "IntDir").text = "$(SolutionDir)obj\\$(Platform)\\$(Configuration)\\"
        ET.SubElement(pg, "TargetName").text = proj_name

    for cfg, plat in [("Debug", "Win32"), ("Release", "Win32"), ("Debug", "x64"), ("Release", "x64")]:
        cond = f"'$(Configuration)|$(Platform)'=='{cfg}|{plat}'"
        idg = ET.SubElement(root, "ItemDefinitionGroup", {"Condition": cond})
        cl = ET.SubElement(idg, "ClCompile")
        ET.SubElement(cl, "WarningLevel").text = "Level3"
        ET.SubElement(cl, "SDLCheck").text = "false"
        ET.SubElement(cl, "ConformanceMode").text = "false"
        ET.SubElement(cl, "AdditionalIncludeDirectories").text = "$(ProjectDir);%(AdditionalIncludeDirectories)"
        ET.SubElement(cl, "PreprocessorDefinitions").text = "_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)"
        ET.SubElement(cl, "LanguageStandard").text = "stdcpp17"
        if cfg == "Debug":
            ET.SubElement(cl, "Optimization").text = "Disabled"
        else:
            ET.SubElement(cl, "Optimization").text = "MaxSpeed"
            ET.SubElement(cl, "FunctionLevelLinking").text = "true"
            ET.SubElement(cl, "IntrinsicFunctions").text = "true"

        lib = ET.SubElement(idg, "Lib")
        if cfg == "Debug":
            ET.SubElement(lib, "AdditionalDependencies").text = "%(AdditionalDependencies)"

    compile_group = ET.SubElement(root, "ItemGroup")
    for file_name in cpp_files:
        ET.SubElement(compile_group, "ClCompile", {"Include": file_name})

    include_group = ET.SubElement(root, "ItemGroup")
    for file_name in h_files:
        ET.SubElement(include_group, "ClInclude", {"Include": file_name})

    ET.SubElement(root, "Import", {"Project": "$(VCTargetsPath)\\Microsoft.Cpp.targets"})

    ET.indent(root, space="  ")
    vcxproj_path.write_text('<?xml version="1.0" encoding="utf-8"?>\n' + ET.tostring(root, encoding="unicode"), encoding="utf-8")
    print(f"[+] {proj_name}.vcxproj -> {vcxproj_path}")
    return proj_guid, proj_name


def write_vcxproj_filters(sdk_root: Path, proj_name: str, cpp_files: list[str], h_files: list[str]):
    filters_path = sdk_root / f"{proj_name}.vcxproj.filters"

    root = ET.Element("Project", {"ToolsVersion": "4.0", "xmlns": "http://schemas.microsoft.com/developer/msbuild/2003"})

    filters = set()
    for file_name in cpp_files + h_files:
        stripped = file_name.lstrip("_")
        pkg = stripped.split("_")[0] if "_" in stripped else "Common"
        filters.add(pkg or "Common")

    filter_group = ET.SubElement(root, "ItemGroup")
    for filter_name in sorted(filters):
        fe = ET.SubElement(filter_group, "Filter", {"Include": filter_name})
        ET.SubElement(fe, "UniqueIdentifier").text = generate_guid()

    cpp_group = ET.SubElement(root, "ItemGroup")
    for file_name in cpp_files:
        stripped = file_name.lstrip("_")
        pkg = stripped.split("_")[0] if "_" in stripped else "Common"
        elem = ET.SubElement(cpp_group, "ClCompile", {"Include": file_name})
        ET.SubElement(elem, "Filter").text = pkg or "Common"

    h_group = ET.SubElement(root, "ItemGroup")
    for file_name in h_files:
        stripped = file_name.lstrip("_")
        pkg = stripped.split("_")[0] if "_" in stripped else "Common"
        elem = ET.SubElement(h_group, "ClInclude", {"Include": file_name})
        ET.SubElement(elem, "Filter").text = pkg or "Common"

    ET.indent(root, space="  ")
    filters_path.write_text('<?xml version="1.0" encoding="utf-8"?>\n' + ET.tostring(root, encoding="unicode"), encoding="utf-8")
    print(f"[+] {proj_name}.vcxproj.filters -> {filters_path}")


def write_sln(sdk_root: Path, proj_name: str, proj_guid: str):
    sln_path = sdk_root / f"{proj_name}.sln"

    sln_content = f"""Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio Version 17
VisualStudioVersion = 17.0.31903.59
MinimumVisualStudioVersion = 10.0.40219.1
Project("{{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}}") = "{proj_name}", "{proj_name}.vcxproj", "{proj_guid}"
EndProject
Global
        GlobalSection(SolutionConfigurationPlatforms) = preSolution
                Debug|x64 = Debug|x64
                Debug|x86 = Debug|x86
                Release|x64 = Release|x64
                Release|x86 = Release|x86
        EndGlobalSection
        GlobalSection(ProjectConfigurationPlatforms) = postSolution
                {proj_guid}.Debug|x64.ActiveCfg = Debug|x64
                {proj_guid}.Debug|x64.Build.0 = Debug|x64
                {proj_guid}.Debug|x86.ActiveCfg = Debug|Win32
                {proj_guid}.Debug|x86.Build.0 = Debug|Win32
                {proj_guid}.Release|x64.ActiveCfg = Release|x64
                {proj_guid}.Release|x64.Build.0 = Release|x64
                {proj_guid}.Release|x86.ActiveCfg = Release|Win32
                {proj_guid}.Release|x86.Build.0 = Release|Win32
        EndGlobalSection
        GlobalSection(SolutionProperties) = preSolution
                HideSolutionNode = FALSE
        EndGlobalSection
        GlobalSection(ExtensibilityGlobals) = postSolution
                SolutionGuid = {generate_guid()}
        EndGlobalSection
EndGlobal
"""

    sln_path.write_text(sln_content, encoding="utf-8")
    print(f"[+] {proj_name}.sln       -> {sln_path}")


def _run_capture(args: list[str]) -> str:
    try:
        completed = subprocess.run(args, capture_output=True, text=True, check=False)
    except OSError:
        return ""
    output = (completed.stdout or "").strip()
    return output


def find_vswhere() -> Path | None:
    for candidate in VSWHERE_CANDIDATES:
        if candidate.exists():
            return candidate
    return None


def find_visual_studio_installation() -> Path | None:
    vswhere = find_vswhere()
    if not vswhere:
        return None
    output = _run_capture(
        [
            str(vswhere),
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property",
            "installationPath",
        ]
    )
    return Path(output) if output else None


# Matches MSVC and Windows SDK version directories. Both ship as dotted-numeric
# names ("14.40.33807.0", "14.51.36231", "10.0.26100.0"), so we accept 3 or 4 parts.
_MS_VERSION_PATTERN = re.compile(r"^\d+\.\d+\.\d+(?:\.\d+)?$")


def _pick_latest_version_with_dirs(versions: list[Path], required_subdirs: list[str]) -> Path | None:
    """Return the highest version (numeric sort) that has all required subdirs."""
    if not versions:
        return None

    sorted_versions = sorted(
        versions,
        key=lambda path: tuple(int(part) for part in path.name.split(".")),
    )
    for candidate in reversed(sorted_versions):
        if all((candidate / sub).is_dir() for sub in required_subdirs):
            return candidate
    return None


def find_msvc_include_dirs(install_path: Path | None) -> list[Path]:
    if not install_path:
        return []

    msvc_root = install_path / "VC" / "Tools" / "MSVC"
    if not msvc_root.exists():
        return []

    versions = [
        path for path in msvc_root.iterdir()
        if path.is_dir() and _MS_VERSION_PATTERN.match(path.name)
    ]
    if not versions:
        return []

    # MSVC versions only need the toolset's own include dir; that always exists
    # alongside the version dir, so any numeric version works.
    latest = sorted(versions, key=lambda path: tuple(int(part) for part in path.name.split(".")))[-1]
    include_dirs = [latest / "include"]

    atlmfc_include = latest / "atlmfc" / "include"
    if atlmfc_include.exists():
        include_dirs.append(atlmfc_include)

    return [path for path in include_dirs if path.exists()]


def find_windows_sdk_include_dirs() -> list[Path]:
    if not WINDOWS_KITS_INCLUDE_ROOT.exists():
        return []

    versions = [
        path for path in WINDOWS_KITS_INCLUDE_ROOT.iterdir()
        if path.is_dir() and _MS_VERSION_PATTERN.match(path.name)
    ]
    if not versions:
        return []

    # Some recent SDK drops (e.g. 10.0.28000.0) ship without ucrt/winrt/cppwinrt.
    # Prefer the highest version that has the full set we need for C++ parsing.
    full_set = ["ucrt", "shared", "um", "winrt", "cppwinrt"]
    latest = _pick_latest_version_with_dirs(versions, full_set)
    if latest is None:
        # Fall back to highest version that has at least ucrt (needed by yvals.h).
        latest = _pick_latest_version_with_dirs(versions, ["ucrt", "shared", "um"])
    if latest is None:
        return []

    candidates = [
        latest / "ucrt",
        latest / "shared",
        latest / "um",
        latest / "winrt",
        latest / "cppwinrt",
    ]
    return [path for path in candidates if path.exists()]


def discover_default_include_dirs() -> list[Path]:
    include_dirs: list[Path] = []

    vc_include = os.environ.get("INCLUDE", "")
    for raw_path in vc_include.split(";"):
        if not raw_path:
            continue
        path = Path(raw_path)
        if path.exists():
            include_dirs.append(path)

    if include_dirs:
        seen = set()
        unique = []
        for path in include_dirs:
            key = str(path).lower()
            if key in seen:
                continue
            seen.add(key)
            unique.append(path)
        return unique

    install_path = find_visual_studio_installation()
    include_dirs.extend(find_msvc_include_dirs(install_path))
    include_dirs.extend(find_windows_sdk_include_dirs())

    seen = set()
    unique = []
    for path in include_dirs:
        key = str(path).lower()
        if key in seen:
            continue
        seen.add(key)
        unique.append(path)
    return unique


def make_compile_command_arguments(sdk_root: Path, source_file: Path, include_dirs: list[Path]) -> list[str]:
    arguments = [
        "cl.exe",
        "/nologo",
        "/TP",
        "/c",
        "/std:c++17",
        "/EHsc",
        "/permissive-",
        "/W3",
        "/DWIN32",
        "/D_WINDOWS",
        "/D_CRT_SECURE_NO_WARNINGS",
        f"/I{sdk_root}",
    ]
    arguments.extend(f"/I{include_dir}" for include_dir in include_dirs)
    arguments.append(str(source_file))
    return arguments


def write_compile_commands(sdk_root: Path, cpp_files: list[str]) -> Path:
    compile_commands_path = sdk_root / "compile_commands.json"
    include_dirs = discover_default_include_dirs()
    entries = []

    for file_name in cpp_files:
        source_file = (sdk_root / file_name).resolve()
        entries.append(
            {
                "directory": str(sdk_root.resolve()),
                "file": str(source_file),
                "arguments": make_compile_command_arguments(sdk_root.resolve(), source_file, include_dirs),
            }
        )

    compile_commands_path.write_text(json.dumps(entries, indent=2), encoding="utf-8")
    print(f"[+] compile_commands.json -> {compile_commands_path}")

    if include_dirs:
        print(f"[*] Added {len(include_dirs)} toolchain include directories for clang-based validation")
    else:
        print("[!] Could not auto-discover MSVC/Windows SDK include directories. compile_commands.json was still generated.")

    return compile_commands_path


def generate_build_files(sdk_root: Path, game_name: str):
    cpp_files, h_files = scan_sdk_files(sdk_root)
    print(f"[*] Found {len(cpp_files)} .cpp files, {len(h_files)} .h files")

    if not cpp_files:
        raise RuntimeError("No .cpp files found. Run SDK generation first.")

    write_cmake(sdk_root, game_name, cpp_files)
    proj_guid, proj_name = write_vcxproj(sdk_root, game_name, cpp_files, h_files)
    write_vcxproj_filters(sdk_root, proj_name, cpp_files, h_files)
    write_sln(sdk_root, proj_name, proj_guid)
    write_compile_commands(sdk_root, cpp_files)
    return proj_name


def main():
    parser = argparse.ArgumentParser(description="Generate build files for UE SDK")
    parser.add_argument("sdk_root", nargs="?", default=None, help="Path to the SDK folder (default: script directory)")
    parser.add_argument("--game-name", default="", help="Game name to use in project naming")
    args = parser.parse_args()

    sdk_root = Path(args.sdk_root).resolve() if args.sdk_root else Path(__file__).parent.resolve()

    if not sdk_root.exists():
        print(f"[!] SDK root does not exist: {sdk_root}")
        sys.exit(1)

    print(f"[*] Scanning SDK folder: {sdk_root}")
    try:
        proj_name = generate_build_files(sdk_root, args.game_name)
    except RuntimeError as exc:
        print(f"[!] {exc}")
        sys.exit(1)

    print("\n[*] Done! You can now:")
    print(f"    1. Open {proj_name}.sln in Visual Studio")
    print("    2. Run clang-tidy against compile_commands.json")
    print("    3. Or build from command line: cmake -B build -S . && cmake --build build --config Release")


if __name__ == "__main__":
    main()
