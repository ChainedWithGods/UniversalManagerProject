#include "CppModAssets.h"

#include "CppModReflection.h"
#include "CppModValue.h"

#include "../core/EngineDiscovery.h"
#include "../core/Logger.h"
#include "../core/NameResolution.h"
#include "../core/ObjectUtils.h"
#include "../crypto/PakVerifier.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace Avalonium::CppMods
{
    namespace
    {
        std::mutex g_IndexMutex;
        std::unordered_set<std::string> g_ModPackages;
        bool g_IndexBuilt = false;

        std::string Lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        std::string NormalizePackagePath(std::string path)
        {
            std::replace(path.begin(), path.end(), '\\', '/');
            path = Lower(std::move(path));
            for (const char* ext : {".uasset", ".umap"})
            {
                const size_t length = std::char_traits<char>::length(ext);
                if (path.size() > length &&
                    path.compare(path.size() - length, length, ext) == 0)
                {
                    path.resize(path.size() - length);
                    break;
                }
            }
            return path;
        }

        std::string PackageFromPakPath(const std::string& fullPath)
        {
            std::string path = fullPath;
            std::replace(path.begin(), path.end(), '\\', '/');
            path = Lower(std::move(path));
            size_t content = path.find("/content/");
            if (content != std::string::npos)
                return NormalizePackagePath("/game/" + path.substr(content + 9));
            size_t project = path.find("project/");
            if (project != std::string::npos)
                return NormalizePackagePath("/game/" + path.substr(project));
            return {};
        }

        std::string AssetPathToPackage(const std::string& assetPath)
        {
            std::string path = assetPath;
            const size_t open = path.find('\'');
            if (open != std::string::npos)
            {
                const size_t close = path.rfind('\'');
                if (close != std::string::npos && close > open)
                    path = path.substr(open + 1, close - open - 1);
            }

            path = NormalizePackagePath(std::move(path));
            if (path.empty()) return {};

            if (path[0] == '/')
            {

                const size_t slash = path.rfind('/');
                const size_t dot = path.find('.', slash == std::string::npos ? 0 : slash);
                if (dot != std::string::npos) path.resize(dot);
                return path;
            }
            return PackageFromPakPath(path);
        }

        std::filesystem::path GameContentPaks()
        {
            wchar_t exePath[MAX_PATH] = {};
            if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) return {};
            std::filesystem::path exe = exePath;
            std::filesystem::path game = exe.parent_path().parent_path().parent_path();
            return game / "Content" / "Paks";
        }

        void AddContainer(const std::filesystem::path& containerPath)
        {
            const std::string extension = Lower(containerPath.extension().string());
            const bool isUtoc = extension == ".utoc";
            Crypto::PakInfo info = isUtoc
                                       ? Crypto::PakVerifier::ParseUtocFile(containerPath.string())
                                       : Crypto::PakVerifier::ParsePakFile(containerPath.string());
            if (!info.found_pak) return;
            if (info.is_encrypted)
            {
                EngineDiscovery::WriteLogLine(
                    ("[CppMod] Mod container " + containerPath.filename().string() +
                     ": encrypted index, skipped")
                        .c_str());
                return;
            }

            Crypto::ReportOptions options;
            options.collect_all_paths = true;
            options.quiet = true;
            options.max_samples = 0;
            options.max_entries = 0;
            options.max_extensions = 0;

            std::vector<Crypto::ArchiveReport> reports;
            Crypto::ReportTotals totals;
            Crypto::PakVerifier::BuildArchiveReports(nullptr, {info}, options, reports, totals);

            size_t before = g_ModPackages.size();
            for (const Crypto::ArchiveReport& report : reports)
            {

                const std::vector<std::string>& paths =
                    isUtoc ? report.utoc_directory.all_paths : report.directory.all_paths;
                for (const std::string& path : paths)
                {
                    const std::string package = PackageFromPakPath(path);
                    if (!package.empty()) g_ModPackages.insert(package);
                }
            }
            if (g_ModPackages.size() != before)
            {
                std::string line = "[CppMod] Mod container ";
                line += containerPath.filename().string();
                line += ": ";
                line += std::to_string(g_ModPackages.size() - before);
                line += " packages";
                EngineDiscovery::WriteLogLine(line.c_str());
            }
        }

        bool IsModRootName(const std::string& name)
        {
            const std::string lower = Lower(name);
            return lower == "~mods" || lower == "logicmods" || lower == "mods";
        }

        void BuildIndex()
        {
            const std::filesystem::path paks = GameContentPaks();
            std::error_code error;
            if (!std::filesystem::is_directory(paks, error))
            {
                EngineDiscovery::WriteLogLine(
                    "[CppMod] Mod asset index: Content/Paks was not found");
                return;
            }

            size_t rootCount = 0;
            size_t pakCount = 0;
            size_t utocCount = 0;
            std::string roots;

            for (std::filesystem::directory_iterator it(paks, error), end;
                 it != end; it.increment(error))
            {
                if (error) break;
                std::error_code typeError;
                if (!it->is_directory(typeError)) continue;
                const std::string name = it->path().filename().string();
                if (!IsModRootName(name)) continue;

                ++rootCount;
                if (!roots.empty()) roots += ", ";
                roots += name;

                const size_t before = g_ModPackages.size();
                size_t rootPaks = 0;
                size_t rootUtocs = 0;
                std::error_code walkError;
                for (std::filesystem::recursive_directory_iterator inner(it->path(), walkError),
                     innerEnd;
                     inner != innerEnd; inner.increment(walkError))
                {
                    if (walkError) break;
                    if (!inner->is_regular_file(walkError)) continue;
                    const std::string extension = Lower(inner->path().extension().string());

                    if (extension == ".pak") ++rootPaks;
                    else if (extension == ".utoc") ++rootUtocs;
                    else continue;
                    AddContainer(inner->path());
                }
                pakCount += rootPaks;
                utocCount += rootUtocs;

                std::string line = "[CppMod] Mod folder ";
                line += name;
                line += ": ";
                line += std::to_string(g_ModPackages.size() - before);
                line += " package(s) from ";
                line += std::to_string(rootPaks);
                line += " pak(s) and ";
                line += std::to_string(rootUtocs);
                line += " utoc(s)";
                EngineDiscovery::WriteLogLine(line.c_str());
            }

            if (rootCount == 0)
            {
                EngineDiscovery::WriteLogLine(
                    "[CppMod] Mod asset index: no mod folder under Content/Paks "
                    "(looked for ~mods, LogicMods, mods)");
                return;
            }

            std::string line = "[CppMod] Mod asset index: ";
            line += std::to_string(g_ModPackages.size());
            line += " packages from ";
            line += std::to_string(pakCount);
            line += " pak(s) and ";
            line += std::to_string(utocCount);
            line += " utoc(s) in ";
            line += roots;
            EngineDiscovery::WriteLogLine(line.c_str());
        }

        struct ImageImportState
        {
            UObject* Library = nullptr;
            bool Ready = false;
            bool Failed = false;
        };

        ImageImportState& ImageImport()
        {
            static ImageImportState import;
            return import;
        }

        UObject* ImportImage(EngineState& state, const char* functionName,
                             const AvaloniumValue (&args)[2])
        {
            ImageImportState& import = ImageImport();
            if (!import.Ready)
            {
                if (import.Failed) return nullptr;

                UClass* libraryClass = FindClass(state, "KismetRenderingLibrary");
                if (libraryClass) import.Library = GetClassDefaultObject(state, libraryClass);
                const bool importersPresent =
                    libraryClass &&
                    FindFunction(state, libraryClass, "ImportFileAsTexture2D") &&
                    FindFunction(state, libraryClass, "ImportBufferAsTexture2D");
                if (!import.Library || !importersPresent)
                {
                    import.Failed = true;
                    EngineDiscovery::WriteLogLine(
                        "[CppMod] Image import: this game has no KismetRenderingLibrary "
                        "image importers; load_image is unavailable");
                    return nullptr;
                }
                import.Ready = true;
            }

            AvaloniumValue result;
            SetNoneValue(result);
            if (!CallFunction(state, import.Library, functionName, args, 2, result))
            {
                return nullptr;
            }
            if (result.kind != AVALONIUM_VALUE_OBJECT || !result.pointer) return nullptr;
            return static_cast<UObject*>(const_cast<void*>(result.pointer));
        }
    }

    bool IsModAsset(EngineState& state, UObject* object)
    {
        if (!object) return false;

        std::lock_guard<std::mutex> lock(g_IndexMutex);
        if (!g_IndexBuilt)
        {
            g_IndexBuilt = true;
            BuildIndex();
        }
        if (g_ModPackages.empty()) return false;

        UObject* outermost = object;
        for (;;)
        {
            UObject* outer = EngineDiscovery::GetObjectOuter(state, outermost);
            if (!outer) break;
            outermost = outer;
        }

        const std::string package = NormalizePackagePath(
            EngineDiscovery::BuildPathName(state, outermost));
        return !package.empty() && g_ModPackages.count(package) != 0;
    }

    bool IsModPackagePath(EngineState& state, const char* assetPath)
    {
        if (!assetPath || !*assetPath) return false;

        std::lock_guard<std::mutex> lock(g_IndexMutex);
        if (!g_IndexBuilt)
        {
            g_IndexBuilt = true;
            BuildIndex();
        }
        if (g_ModPackages.empty()) return false;

        const std::string package = AssetPathToPackage(assetPath);
        return !package.empty() && g_ModPackages.count(package) != 0;
    }

    int32_t ModPackageCount(EngineState& state)
    {
        std::lock_guard<std::mutex> lock(g_IndexMutex);
        if (!g_IndexBuilt)
        {
            g_IndexBuilt = true;
            BuildIndex();
        }
        return static_cast<int32_t>(g_ModPackages.size());
    }

    UObject* ImportImageFile(EngineState& state, const std::string& path)
    {
        if (path.empty()) return nullptr;

        AvaloniumValue args[2];
        SetNoneValue(args[0]);
        args[0].kind = AVALONIUM_VALUE_OBJECT;
        SetStringValue(path, args[1]);
        return ImportImage(state, "ImportFileAsTexture2D", args);
    }

    UObject* ImportImageMemory(EngineState& state, const void* bytes, size_t size)
    {
        if (!bytes || size == 0 || size > 0x7FFFFFFFull) return nullptr;

        struct TArrayView
        {
            const void* Data;
            int32_t Num;
            int32_t Max;
        };
        const TArrayView view{bytes, static_cast<int32_t>(size), static_cast<int32_t>(size)};

        AvaloniumValue args[2];
        SetNoneValue(args[0]);
        args[0].kind = AVALONIUM_VALUE_OBJECT;
        SetNoneValue(args[1]);
        args[1].kind = AVALONIUM_VALUE_STRUCT;
        args[1].size = static_cast<int32_t>(sizeof(view));
        args[1].pointer = &view;
        return ImportImage(state, "ImportBufferAsTexture2D", args);
    }
}
