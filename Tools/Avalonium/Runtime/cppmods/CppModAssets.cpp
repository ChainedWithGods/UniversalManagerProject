#include "CppModAssets.h"

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

        std::filesystem::path GameContentPaks()
        {
            wchar_t exePath[MAX_PATH] = {};
            if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) return {};
            std::filesystem::path exe = exePath;
            std::filesystem::path game = exe.parent_path().parent_path().parent_path();
            return game / "Content" / "Paks";
        }

        void AddPak(const std::filesystem::path& pakPath)
        {
            Crypto::PakInfo info = Crypto::PakVerifier::ParsePakFile(pakPath.string());
            if (!info.found_pak || info.is_encrypted) return;

            Crypto::ReportOptions options;
            options.collect_all_paths = true;
            options.max_samples = 0;
            options.max_entries = 0;
            options.max_extensions = 0;

            std::vector<Crypto::ArchiveReport> reports;
            Crypto::ReportTotals totals;
            Crypto::PakVerifier::BuildArchiveReports(nullptr, {info}, options, reports, totals);

            size_t before = g_ModPackages.size();
            for (const Crypto::ArchiveReport& report : reports)
            {
                for (const std::string& path : report.directory.all_paths)
                {
                    const std::string package = PackageFromPakPath(path);
                    if (!package.empty()) g_ModPackages.insert(package);
                }
            }
            if (g_ModPackages.size() != before)
            {
                std::string line = "[CppMod] Mod pak ";
                line += pakPath.filename().string();
                line += ": ";
                line += std::to_string(g_ModPackages.size() - before);
                line += " packages";
                EngineDiscovery::WriteLogLine(line.c_str());
            }
        }

        void BuildIndex()
        {
            const std::filesystem::path paks = GameContentPaks();
            const std::filesystem::path mods = paks / "~mods";
            std::error_code error;
            if (!std::filesystem::is_directory(mods, error))
            {
                EngineDiscovery::WriteLogLine(
                    "[CppMod] Mod asset index: Content/Paks/~mods was not found");
                return;
            }

            size_t pakCount = 0;
            for (std::filesystem::recursive_directory_iterator it(mods, error), end;
                 it != end; it.increment(error))
            {
                if (error) break;
                if (!it->is_regular_file(error)) continue;
                std::string extension = Lower(it->path().extension().string());
                if (extension != ".pak") continue;
                ++pakCount;
                AddPak(it->path());
            }

            std::string line = "[CppMod] Mod asset index: ";
            line += std::to_string(g_ModPackages.size());
            line += " packages from ";
            line += std::to_string(pakCount);
            line += " pak(s)";
            EngineDiscovery::WriteLogLine(line.c_str());
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
}
