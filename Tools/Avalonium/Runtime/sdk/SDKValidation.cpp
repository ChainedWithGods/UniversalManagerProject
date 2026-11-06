#include "SDKValidation.h"
#include "SDKGenerator.h"
#include "../core/Logger.h"
#include "../core/ObjectArray.h"
#include "../core/NameResolution.h"
#include "../core/ObjectUtils.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <algorithm>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <cstdint>
#include <cctype>
#include <cstdio>
#include <cstring>

extern "C" {
#include "../utils/SafeMemory.h"
}

namespace SDKValidation {

using EngineState = ::EngineState;
using namespace EngineDiscovery;
using SDKGenerator::SdkLog;

struct SDKMetadataSymbol {
    std::string Kind;
    std::string PackageName;
    std::string Name;
    std::string CppName;
    std::string FullPath;
    std::string SuperName;
    int32_t Size = 0;
    int32_t PropertyCount = 0;
    int32_t ValueCount = 0;
};

static std::mutex g_ValidationStateMutex;
static std::unordered_set<std::string> g_ValidationPathsInFlight;
static std::unordered_set<std::string> g_ValidationPathsCompleted;

static std::string NormalizeValidationPath(std::string path) {
    std::replace(path.begin(), path.end(), '/', '\\');
    while (path.size() > 3 && !path.empty() && path.back() == '\\') {
        path.pop_back();
    }
    std::transform(path.begin(), path.end(), path.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return path;
}

static std::string ExtractJsonString(const std::string& line, const std::string& key) {
    std::string search = "\"" + key + "\": \"";
    size_t pos = line.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    size_t endPos = line.find("\"", pos);
    if (endPos == std::string::npos) return "";
    return line.substr(pos, endPos - pos);
}

static int32_t ExtractJsonInt(const std::string& line, const std::string& key) {
    std::string search = "\"" + key + "\": ";
    size_t pos = line.find(search);
    if (pos == std::string::npos) return 0;
    pos += search.length();
    size_t endPos = line.find_first_of(",\n\r }", pos);
    if (endPos == std::string::npos) endPos = line.length();
    try {
        return std::stoi(line.substr(pos, endPos - pos));
    } catch (...) {
        return 0;
    }
}

static std::unordered_map<std::string, SDKMetadataSymbol> LoadSDKMetadata(const std::string& sdkPath) {
    std::unordered_map<std::string, SDKMetadataSymbol> symbols;
    std::string jsonPath = sdkPath + "\\SDKMetadata.json";
    Log("[SDKValidation] Looking for SDKMetadata.json at: %s\n", jsonPath.c_str());
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        Log("[SDKValidation] ERROR: Couldn't find SDKMetadata.json at the specified path.\n");
        Log("[SDKValidation]   Checked: %s\n", jsonPath.c_str());
        Log("[SDKValidation]   Make sure SDKPath in Avalonium.ini points to the folder containing SDKMetadata.json.\n");
        return symbols;
    }

    std::string line;
    SDKMetadataSymbol currentSymbol;
    bool inSymbol = false;

    while (std::getline(file, line)) {
        if (line.find("\"symbols\": [") != std::string::npos) {
            inSymbol = false;
        }
        if (line.find("{") != std::string::npos) {
            inSymbol = true;
            currentSymbol = SDKMetadataSymbol();
        }

        if (inSymbol) {
            if (line.find("\"kind\":") != std::string::npos) currentSymbol.Kind = ExtractJsonString(line, "kind");
            else if (line.find("\"package\":") != std::string::npos) currentSymbol.PackageName = ExtractJsonString(line, "package");
            else if (line.find("\"name\":") != std::string::npos) currentSymbol.Name = ExtractJsonString(line, "name");
            else if (line.find("\"cpp_name\":") != std::string::npos) currentSymbol.CppName = ExtractJsonString(line, "cpp_name");
            else if (line.find("\"full_path\":") != std::string::npos) currentSymbol.FullPath = ExtractJsonString(line, "full_path");
            else if (line.find("\"super\":") != std::string::npos) currentSymbol.SuperName = ExtractJsonString(line, "super");
            else if (line.find("\"size\":") != std::string::npos) currentSymbol.Size = ExtractJsonInt(line, "size");
            else if (line.find("\"property_count\":") != std::string::npos) currentSymbol.PropertyCount = ExtractJsonInt(line, "property_count");
            else if (line.find("\"value_count\":") != std::string::npos) currentSymbol.ValueCount = ExtractJsonInt(line, "value_count");
        }

        if (line.find("}") != std::string::npos && inSymbol) {
            if (!currentSymbol.FullPath.empty()) {
                symbols[currentSymbol.FullPath] = currentSymbol;
            }
            inSymbol = false;
        }
    }

    return symbols;
}

struct ValidationResult {
    int PassCount = 0;
    int WarnCount = 0;
    int FailCount = 0;
    int NewCount = 0;
    std::string LogBuffer;

    void AddPass(const std::string& msg) {
        PassCount++;
        LogBuffer += "[PASS] " + msg + "\n";
    }
    void AddWarn(const std::string& msg) {
        WarnCount++;
        LogBuffer += "[WARN] " + msg + "\n";
    }
    void AddFail(const std::string& msg) {
        FailCount++;
        LogBuffer += "[FAIL] " + msg + "\n";
    }
    void AddNew(const std::string& msg) {
        NewCount++;

    }
};

static void WriteLogFile(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    if (out.is_open()) {
        out << content;
    }
}

static DWORD WINAPI ValidationThreadProc(LPVOID lpParam) {
    struct ThreadData { EngineState state; std::string path; std::string normalizedPath; };
    ThreadData* data = reinterpret_cast<ThreadData*>(lpParam);
    EngineState state = data->state;
    std::string sdkPath = data->path;
    std::string normalizedPath = data->normalizedPath;
    delete data;

    Log("[SDKValidation] Thread started, loading SDKMetadata.json...\n");

    auto sdkSymbols = LoadSDKMetadata(sdkPath);
    if (sdkSymbols.empty()) {
        WriteLogFile(sdkPath + "\\SDKValidation.log", "Failed to load SDKMetadata.json or no symbols found. Validation aborted.\n");
        std::lock_guard<std::mutex> lock(g_ValidationStateMutex);
        g_ValidationPathsInFlight.erase(normalizedPath);
        g_ValidationPathsCompleted.insert(normalizedPath);
        return 0;
    }

    Log("[SDKValidation] Loaded %zu symbols from SDK metadata. Validating against live memory...\n", sdkSymbols.size());

    ValidationResult result;
    result.LogBuffer = "=== Avalonium SDK Validation ===\n";
    result.LogBuffer += "SDK Path : " + sdkPath + "\n";
    result.LogBuffer += "Symbols  : " + std::to_string(sdkSymbols.size()) + "\n\n";

    std::unordered_set<std::string> validatedPaths;

    int32_t liveObjectCount = GetObjectCount(state);

    for (int32_t i = 0; i < liveObjectCount; ++i) {
        UObject* obj = GetObjectByIndex(state, i);
        if (!obj) continue;

        UClass* cls = GetObjectClass(state, obj);
        if (!cls) continue;

        bool isClass = false, isScriptStruct = false, isEnum = false;

        UStruct* scan = (UStruct*)cls;
        for (int depth = 0; depth < 10 && scan; depth++) {
            std::string scanName = GetObjectNameStr(state, (UObject*)scan);
            if (scanName == "Class") {
                isClass = true;
                break;
            } else if (scanName == "ScriptStruct") {
                isScriptStruct = true;
                break;
            } else if (scanName == "Enum") {
                isEnum = true;
                break;
            }
            if (state.UStruct_SuperOff >= 0) {
                void* superPtr = nullptr;
                SafeMem_Read((uint8_t*)scan + state.UStruct_SuperOff, &superPtr, 8);
                scan = (UStruct*)superPtr;
            } else {
                scan = nullptr;
            }
        }

        if (!isClass && !isScriptStruct && !isEnum) continue;

        std::string objectName = GetObjectNameStr(state, obj);
        if (objectName.empty() || objectName.find("Default__") == 0 || objectName.find("SKEL_") == 0 || objectName.find("REINST_") == 0) continue;

        std::string fullPath = BuildFullName(state, obj);
        if (fullPath.empty()) continue;

        auto it = sdkSymbols.find(fullPath);
        if (it == sdkSymbols.end()) {
            result.AddNew(fullPath);
            continue;
        }

        validatedPaths.insert(fullPath);
        const SDKMetadataSymbol& sdkSym = it->second;

        std::string warnReasons = "";

        if (isClass || isScriptStruct) {

            int32_t liveSize = 0;
            if (state.UStruct_PropertiesSizeOff > 0) {
                liveSize = *(int32_t*)((uint8_t*)obj + state.UStruct_PropertiesSizeOff);
            }
            if (liveSize > 0 && sdkSym.Size > 0 && liveSize != sdkSym.Size) {
                warnReasons += "Size mismatch (SDK=" + std::to_string(sdkSym.Size) + ", Live=" + std::to_string(liveSize) + ") ";
            }

            int32_t livePropCount = 0;
            int32_t offsetOff = state.FProperty_OffsetOff >= 0 ? state.FProperty_OffsetOff : 0x4C;

            if (state.bUseFProperty && state.UStruct_ChildPropsOff > 0) {
                void* ffield = *(void**)((uint8_t*)obj + state.UStruct_ChildPropsOff);
                while (ffield) {
                    int32_t propOffset = 0;
                    SafeMem_Read((uint8_t*)ffield + offsetOff, &propOffset, 4);
                    if (propOffset > 0) livePropCount++;
                    ffield = GetFFieldNext(state, ffield);
                }
            } else if (!state.bUseFProperty && state.UStruct_ChildrenOff > 0) {
                int32_t legacyOffOff = state.UProperty_OffsetOff >= 0 ? state.UProperty_OffsetOff : 0x44;
                void* ufield = *(void**)((uint8_t*)obj + state.UStruct_ChildrenOff);
                while (ufield) {
                    std::string uclsName = GetObjectNameStr(state, GetObjectClass(state, (UObject*)ufield));
                    if (uclsName.find("Property") != std::string::npos) {
                        int32_t propOffset = 0;
                        SafeMem_Read((uint8_t*)ufield + legacyOffOff, &propOffset, 4);
                        if (propOffset > 0) livePropCount++;
                    }
                    ufield = GetFieldNext(state, ufield);
                }
            }

            if (sdkSym.PropertyCount > 0 && livePropCount > 0 && sdkSym.PropertyCount != livePropCount) {
                warnReasons += "PropertyCount mismatch (SDK=" + std::to_string(sdkSym.PropertyCount) + ", Live=" + std::to_string(livePropCount) + ") ";
            }
        }
        else if (isEnum) {

        }

        if (warnReasons.empty()) {
            result.AddPass(fullPath + " verified");
        } else {
            result.AddWarn(fullPath + " -> " + warnReasons);
        }
    }

    for (const auto& pair : sdkSymbols) {
        if (pair.second.Kind == "function") continue;
        if (validatedPaths.find(pair.first) == validatedPaths.end()) {
            result.AddFail(pair.first + " NOT FOUND in live memory (removed or renamed)");
        }
    }

    result.LogBuffer += "\nSummary: " + std::to_string(result.PassCount) + " PASS | "
                      + std::to_string(result.WarnCount) + " WARN | "
                      + std::to_string(result.FailCount) + " FAIL\n";
    if (result.NewCount > 0) {
        result.LogBuffer += "[INFO] " + std::to_string(result.NewCount) + " structural types in live memory are not in SDK -> re-dump recommended\n";
    }

    if (result.WarnCount > 0 || result.FailCount > 0 || result.NewCount > 50) {
        result.LogBuffer += "Verdict: SDK partially stale. Re-dump recommended.\n";
    } else {
        result.LogBuffer += "Verdict: SDK matches live memory perfectly.\n";
    }

    std::string logFilePath = sdkPath + "\\SDKValidation.log";
    WriteLogFile(logFilePath, result.LogBuffer);
    Log("[SDKValidation] Validation complete. Wrote %s\n", logFilePath.c_str());

    {
        std::lock_guard<std::mutex> lock(g_ValidationStateMutex);
        g_ValidationPathsInFlight.erase(normalizedPath);
        g_ValidationPathsCompleted.insert(normalizedPath);
    }

    return 0;
}

void StartSDKValidationThread(const EngineState& state, const std::string& sdkPath) {
    const std::string normalizedPath = NormalizeValidationPath(sdkPath);
    {
        std::lock_guard<std::mutex> lock(g_ValidationStateMutex);
        if (normalizedPath.empty()) {
            Log("[SDKValidation] Skipping validation request because the SDK path is empty.\n");
            return;
        }
        if (g_ValidationPathsInFlight.find(normalizedPath) != g_ValidationPathsInFlight.end()) {
            Log("[SDKValidation] Validation already running for %s - skipping duplicate request.\n", sdkPath.c_str());
            return;
        }
        if (g_ValidationPathsCompleted.find(normalizedPath) != g_ValidationPathsCompleted.end()) {
            Log("[SDKValidation] Validation already completed for %s in this session - skipping duplicate request.\n", sdkPath.c_str());
            return;
        }
        g_ValidationPathsInFlight.insert(normalizedPath);
    }

    struct ThreadData { EngineState state; std::string path; std::string normalizedPath; };
    ThreadData* data = new ThreadData{ state, sdkPath, normalizedPath };
    HANDLE thread = CreateThread(nullptr, 0, ValidationThreadProc, data, 0, nullptr);
    if (!thread) {
        std::lock_guard<std::mutex> lock(g_ValidationStateMutex);
        g_ValidationPathsInFlight.erase(normalizedPath);
        Log("[SDKValidation] WARNING: Failed to spawn validation thread. Error: %lu\n", GetLastError());
        delete data;
        return;
    }
    CloseHandle(thread);
}

namespace {
struct SDKGenerationThreadData {
    std::string sdkPath;
    std::string gameName;
    std::string libclangPath;
};

using CXIndex = void*;
using CXTranslationUnit = void*;
using CXDiagnostic = void*;
using CXFile = void*;
struct CXString { const void* data; unsigned privateFlags; };
struct CXSourceLocation { const void* ptrData[2]; unsigned intData; };
struct CXCursor { unsigned kind; int xdata; const void* data[3]; };
using CXClientData = void*;
using CXCursorVisitor = unsigned(__cdecl*)(CXCursor, CXCursor, CXClientData);

constexpr unsigned kDiagnosticError = 3;
constexpr unsigned kDiagnosticFatal = 4;
constexpr unsigned kChildVisitContinue = 1;
constexpr unsigned kChildVisitRecurse = 2;

static std::wstring Utf8OrAnsiToWide(const std::string& value) {
    if (value.empty()) return {};
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, nullptr, 0);
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (length <= 0) {

        codePage = CP_ACP;
        flags = 0;
        length = MultiByteToWideChar(codePage, flags, value.c_str(), -1, nullptr, 0);
    }
    if (length <= 0) return {};
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(codePage, flags, value.c_str(), -1, result.data(), length);
    result.pop_back();
    return result;
}

static std::filesystem::path PathFromUtf8OrAnsi(const std::string& value) {
    return std::filesystem::path(Utf8OrAnsiToWide(value));
}

static std::string PathToUtf8(const std::filesystem::path& value) {
    const std::wstring wide = value.wstring();
    if (wide.empty()) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), result.data(), length, nullptr, nullptr);
    return result;
}

struct NativeLibclang {
    HMODULE module = nullptr;
    std::string loadedPath;
    CXIndex(__cdecl* createIndex)(int, int) = nullptr;
    void(__cdecl* disposeIndex)(CXIndex) = nullptr;
    int(__cdecl* parseTranslationUnit)(CXIndex, const char*, const char* const*, int, void*, unsigned, unsigned, CXTranslationUnit*) = nullptr;
    void(__cdecl* disposeTranslationUnit)(CXTranslationUnit) = nullptr;
    unsigned(__cdecl* getNumDiagnostics)(CXTranslationUnit) = nullptr;
    CXDiagnostic(__cdecl* getDiagnostic)(CXTranslationUnit, unsigned) = nullptr;
    unsigned(__cdecl* getDiagnosticSeverity)(CXDiagnostic) = nullptr;
    CXString(__cdecl* formatDiagnostic)(CXDiagnostic, unsigned) = nullptr;
    void(__cdecl* disposeDiagnostic)(CXDiagnostic) = nullptr;
    const char* (__cdecl* getCString)(CXString) = nullptr;
    void(__cdecl* disposeString)(CXString) = nullptr;
    CXSourceLocation(__cdecl* getDiagnosticLocation)(CXDiagnostic) = nullptr;
    void(__cdecl* getFileLocation)(CXSourceLocation, CXFile*, unsigned*, unsigned*, unsigned*) = nullptr;
    CXString(__cdecl* getFileName)(CXFile) = nullptr;
    CXCursor(__cdecl* getTranslationUnitCursor)(CXTranslationUnit) = nullptr;
    unsigned(__cdecl* visitChildren)(CXCursor, CXCursorVisitor, CXClientData) = nullptr;

    ~NativeLibclang() {
        if (module) FreeLibrary(module);
    }

    template <typename T>
    bool Bind(T& target, const char* name) {
        target = reinterpret_cast<T>(GetProcAddress(module, name));
        return target != nullptr;
    }

    bool Load(const std::string& configuredPath, std::string& error) {
        namespace fs = std::filesystem;
        std::vector<fs::path> candidates;
        auto appendCandidate = [&candidates](const std::string& value) {
            if (value.empty()) return;
            fs::path path(value);
            candidates.push_back(path.filename() == "libclang.dll" ? path : path / "libclang.dll");
        };
        appendCandidate(configuredPath);
        char environmentPath[MAX_PATH] = {};
        if (GetEnvironmentVariableA("LIBCLANG_PATH", environmentPath, MAX_PATH)) appendCandidate(environmentPath);
        if (GetEnvironmentVariableA("CLANG_LIBRARY_PATH", environmentPath, MAX_PATH)) appendCandidate(environmentPath);
        const char* programFiles = std::getenv("ProgramFiles");
        const char* programFilesX86 = std::getenv("ProgramFiles(x86)");
        for (const char* root : { programFiles, programFilesX86 }) {
            if (!root) continue;
            for (const char* version : { "18", "2022" }) {
                for (const char* edition : { "Community", "Professional", "Enterprise", "BuildTools" }) {
                    candidates.emplace_back(fs::path(root) / "Microsoft Visual Studio" / version / edition / "VC" / "Tools" / "Llvm" / "x64" / "bin" / "libclang.dll");
                }
            }
            candidates.emplace_back(fs::path(root) / "LLVM" / "bin" / "libclang.dll");
        }

        std::string searched;
        for (const fs::path& path : candidates) {
            std::error_code ec;
            if (!fs::is_regular_file(path, ec)) continue;
            module = LoadLibraryA(path.string().c_str());
            if (module) {
                loadedPath = path.string();
                break;
            }
            searched += path.string() + " (LoadLibrary error " + std::to_string(GetLastError()) + "); ";
        }
        if (!module) {
            error = "Could not load libclang.dll. " + searched;
            return false;
        }

        const bool bound =
            Bind(createIndex, "clang_createIndex") && Bind(disposeIndex, "clang_disposeIndex") &&
            Bind(parseTranslationUnit, "clang_parseTranslationUnit2FullArgv") && Bind(disposeTranslationUnit, "clang_disposeTranslationUnit") &&
            Bind(getNumDiagnostics, "clang_getNumDiagnostics") && Bind(getDiagnostic, "clang_getDiagnostic") &&
            Bind(getDiagnosticSeverity, "clang_getDiagnosticSeverity") && Bind(formatDiagnostic, "clang_formatDiagnostic") &&
            Bind(disposeDiagnostic, "clang_disposeDiagnostic") && Bind(getCString, "clang_getCString") && Bind(disposeString, "clang_disposeString") &&
            Bind(getDiagnosticLocation, "clang_getDiagnosticLocation") && Bind(getFileLocation, "clang_getFileLocation") &&
            Bind(getFileName, "clang_getFileName") && Bind(getTranslationUnitCursor, "clang_getTranslationUnitCursor") &&
            Bind(visitChildren, "clang_visitChildren");
        if (!bound) {
            error = "libclang.dll is missing a required C API entry point.";
            return false;
        }
        return true;
    }

    std::string ToString(CXString value) const {
        const char* text = getCString(value);
        std::string result = text ? text : "";
        disposeString(value);
        return result;
    }
};

static std::string EscapeJson(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (unsigned char c : value) {
        switch (c) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (c < 0x20) {
                char buffer[7] = {};
                snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                escaped += buffer;
            } else {
                escaped += static_cast<char>(c);
            }
            break;
        }
    }
    return escaped;
}

static bool IsVersionDirectory(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    return !name.empty() && std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isdigit(c) || c == '.'; });
}

static void AppendLatestSubdirectory(const std::filesystem::path& root, std::vector<std::filesystem::path>& includes) {
    namespace fs = std::filesystem;
    std::error_code ec;
    std::vector<fs::path> versions;
    for (const fs::directory_entry& entry : fs::directory_iterator(root, ec)) {
        if (!ec && entry.is_directory(ec) && IsVersionDirectory(entry.path())) versions.push_back(entry.path());
    }
    if (versions.empty()) return;
    std::sort(versions.begin(), versions.end());

    const fs::path* selected = nullptr;
    for (auto it = versions.rbegin(); it != versions.rend(); ++it) {
        const bool hasUcrt = fs::is_directory(*it / "ucrt", ec);
        const bool hasShared = fs::is_directory(*it / "shared", ec);
        const bool hasUm = fs::is_directory(*it / "um", ec);
        if (hasUcrt && hasShared && hasUm) {
            selected = &*it;
            break;
        }
    }
    if (!selected) selected = &versions.back();
    for (const char* folder : { "ucrt", "shared", "um", "winrt", "cppwinrt" }) {
        const fs::path include = *selected / folder;
        if (fs::is_directory(include, ec)) includes.push_back(include);
    }
}

static std::vector<std::string> DiscoverIncludeDirectories(const std::string& libclangPath) {
    namespace fs = std::filesystem;
    std::vector<fs::path> paths;
    char includeEnvironment[32767] = {};
    const DWORD copied = GetEnvironmentVariableA("INCLUDE", includeEnvironment, static_cast<DWORD>(sizeof(includeEnvironment)));
    if (copied > 0 && copied < sizeof(includeEnvironment)) {
        std::stringstream stream(includeEnvironment);
        std::string item;
        while (std::getline(stream, item, ';')) {
            std::error_code ec;
            if (!item.empty() && fs::is_directory(item, ec)) paths.emplace_back(item);
        }
    }

    std::error_code ec;
    fs::path clangPath(libclangPath);
    if (clangPath.filename() != "libclang.dll") clangPath /= "libclang.dll";

    const fs::path vcRoot = clangPath.parent_path().parent_path().parent_path().parent_path().parent_path();
    const fs::path msvcRoot = vcRoot / "Tools" / "MSVC";
    if (fs::is_directory(msvcRoot, ec)) {
        std::vector<fs::path> versions;
        for (const fs::directory_entry& entry : fs::directory_iterator(msvcRoot, ec)) {
            if (!ec && entry.is_directory(ec) && IsVersionDirectory(entry.path())) versions.push_back(entry.path());
        }
        if (!versions.empty()) {
            std::sort(versions.begin(), versions.end());
            const fs::path include = versions.back() / "include";
            if (fs::is_directory(include, ec)) paths.push_back(include);
        }
    }
    const char* kitsRoot = std::getenv("ProgramFiles(x86)");
    if (kitsRoot) AppendLatestSubdirectory(fs::path(kitsRoot) / "Windows Kits" / "10" / "Include", paths);

    std::vector<std::string> unique;
    for (const fs::path& path : paths) {
        const std::string value = path.string();
        const auto duplicate = std::find_if(unique.begin(), unique.end(), [&value](const std::string& existing) {
            return _stricmp(existing.c_str(), value.c_str()) == 0;
        });
        if (duplicate == unique.end()) unique.push_back(value);
    }
    return unique;
}

static unsigned __cdecl CountAstCursor(CXCursor, CXCursor, CXClientData clientData) {
    ++*static_cast<unsigned long long*>(clientData);
    return kChildVisitRecurse;
}

struct AstTranslationUnitResult {
    std::string relativePath;
    unsigned long long cursorCount = 0;
    std::vector<std::string> errors;
};

static AstTranslationUnitResult ParseAstTranslationUnit(NativeLibclang& clang, CXIndex index,
                                                         const std::filesystem::path& sdkRoot,
                                                         const std::filesystem::path& source,
                                                         const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    AstTranslationUnitResult result;
    std::error_code ec;
    result.relativePath = PathToUtf8(fs::relative(source, sdkRoot, ec));
    if (ec) result.relativePath = PathToUtf8(source.filename());
    if (source.filename().string().rfind("AvaloniumSdkAst_", 0) == 0) {
        result.relativePath = "<native-libclang-unity>";
    }

    std::vector<std::string> command = { "clang" };
    command.insert(command.end(), args.begin(), args.end());
    std::vector<const char*> argv;
    argv.reserve(command.size());
    for (const std::string& argument : command) argv.push_back(argument.c_str());

    CXTranslationUnit translationUnit = nullptr;

    constexpr unsigned kDetailedPreprocessingRecord = 0x01;
    const std::string sourceUtf8 = PathToUtf8(source);
    const int parseResult = clang.parseTranslationUnit(
        index, sourceUtf8.c_str(), argv.data(), static_cast<int>(argv.size()),
        nullptr, 0, kDetailedPreprocessingRecord, &translationUnit);
    if (parseResult != 0 || !translationUnit) {
        result.errors.push_back(result.relativePath + ": libclang could not create a translation unit (error " + std::to_string(parseResult) + ").");
        return result;
    }
    for (unsigned i = 0; i < clang.getNumDiagnostics(translationUnit); ++i) {
        CXDiagnostic diagnostic = clang.getDiagnostic(translationUnit, i);
        const unsigned severity = clang.getDiagnosticSeverity(diagnostic);
        if (severity >= kDiagnosticError && severity <= kDiagnosticFatal) {
            CXFile file = nullptr;
            unsigned line = 0, column = 0, offset = 0;
            clang.getFileLocation(clang.getDiagnosticLocation(diagnostic), &file, &line, &column, &offset);
            const std::string fileName = file ? clang.ToString(clang.getFileName(file)) : result.relativePath;
            result.errors.push_back(fileName + ":" + std::to_string(line) + ":" + std::to_string(column) + ": " + clang.ToString(clang.formatDiagnostic(diagnostic, 0)));
        }
        clang.disposeDiagnostic(diagnostic);
    }
    if (result.errors.empty()) clang.visitChildren(clang.getTranslationUnitCursor(translationUnit), CountAstCursor, &result.cursorCount);
    clang.disposeTranslationUnit(translationUnit);
    return result;
}

static void WriteAstGenerationLog(const std::filesystem::path& path, const std::vector<std::string>& lines) {
    std::ofstream output(path, std::ios::trunc);
    if (!output) return;
    for (const std::string& line : lines) output << line << '\n';
}

static void WriteAstModel(const std::filesystem::path& sdkRoot, const std::vector<AstTranslationUnitResult>& results) {
    std::ofstream output(sdkRoot / "SDKAstModel.json", std::ios::trunc);
    if (!output) return;
    output << "{\n  \"format\": 2,\n  \"backend\": \"native-libclang\",\n  \"translation_units\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const AstTranslationUnitResult& result = results[i];
        output << "    {\"file\": \"" << EscapeJson(result.relativePath) << "\", \"ast_cursors\": " << result.cursorCount << "}";
        output << (i + 1 == results.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
}

static std::filesystem::path MakeUnityAstSourcePath() {
    char tempDirectory[MAX_PATH] = {};
    const DWORD length = GetTempPathA(MAX_PATH, tempDirectory);
    if (length == 0 || length >= MAX_PATH) return {};
    return std::filesystem::path(tempDirectory) / ("AvaloniumSdkAst_" + std::to_string(GetCurrentProcessId()) + ".cpp");
}

static bool WriteUnityAstSource(const std::filesystem::path& path,
                                const std::filesystem::path& sdkRoot,
                                const std::vector<std::filesystem::path>& sourceFiles) {
    std::ofstream output(path, std::ios::trunc);
    if (!output) return false;

    output << "#include \"" << PathToUtf8(sdkRoot / "SDK.h") << "\"\n";
    for (const std::filesystem::path& source : sourceFiles) {
        output << "#include \"" << PathToUtf8(source) << "\"\n";
    }
    return output.good();
}

DWORD WINAPI SDKGenerationThreadProc(LPVOID param) {
    auto* data = static_cast<SDKGenerationThreadData*>(param);
    namespace fs = std::filesystem;
    const fs::path sdkRoot = PathFromUtf8OrAnsi(data->sdkPath);
    const std::string gameLabel = data->gameName.empty() ? "<unknown-game>" : data->gameName;
    std::vector<std::string> logLines = {
        "=== Avalonium native libclang SDK generation ===",
        "SDK Path: " + data->sdkPath,
    };
    if (data->sdkPath.empty() || !fs::is_directory(sdkRoot)) {
        logLines.push_back("ERROR: SDK path does not exist or is not a directory.");
        logLines.push_back("Verdict: SETUP FAILED (no SDK files were changed).");
        WriteAstGenerationLog(sdkRoot / "SDKAstGeneration.log", logLines);
        SdkLog("[SDK] Native libclang generation skipped: SDK path is invalid for %s", gameLabel.c_str());
        delete data;
        return 0;
    }

    NativeLibclang clang;
    std::string loadError;
    if (!clang.Load(data->libclangPath, loadError)) {
        logLines.push_back("ERROR: " + loadError);
        logLines.push_back("Verdict: SETUP FAILED (no SDK files were changed).");
        WriteAstGenerationLog(sdkRoot / "SDKAstGeneration.log", logLines);
        SdkLog("[SDK] Native libclang setup failed for %s: %s", gameLabel.c_str(), loadError.c_str());
        delete data;
        return 1;
    }

    const std::vector<std::string> includeDirectories = DiscoverIncludeDirectories(clang.loadedPath);
    std::vector<std::string> arguments = { "-x", "c++", "-std=c++20", "-fms-extensions", "-fms-compatibility", "-fsyntax-only", "-I", PathToUtf8(sdkRoot) };
    for (const std::string& include : includeDirectories) {
        arguments.push_back("-I");
        arguments.push_back(include);
    }

    std::vector<fs::path> sourceFiles;
    std::error_code iteratorError;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(sdkRoot, fs::directory_options::skip_permission_denied, iteratorError)) {
        if (iteratorError || !entry.is_regular_file(iteratorError)) continue;
        if (entry.path().filename().string().rfind(".native-libclang-", 0) == 0) continue;
        const std::string extension = entry.path().extension().string();
        if (_stricmp(extension.c_str(), ".cpp") == 0 || _stricmp(extension.c_str(), ".cxx") == 0 || _stricmp(extension.c_str(), ".cc") == 0) {
            sourceFiles.push_back(entry.path());
        }
    }
    if (!fs::is_regular_file(sdkRoot / "SDK.h")) {
        logLines.push_back("ERROR: SDK.h was not found.");
        logLines.push_back("Verdict: SETUP FAILED (no SDK files were changed).");
        WriteAstGenerationLog(sdkRoot / "SDKAstGeneration.log", logLines);
        delete data;
        return 1;
    }

    const fs::path unitySource = MakeUnityAstSourcePath();
    if (unitySource.empty() || !WriteUnityAstSource(unitySource, sdkRoot, sourceFiles)) {
        logLines.push_back("ERROR: Could not create the temporary native libclang unity source.");
        logLines.push_back("Verdict: SETUP FAILED (no SDK files were changed).");
        WriteAstGenerationLog(sdkRoot / "SDKAstGeneration.log", logLines);
        delete data;
        return 1;
    }

    CXIndex index = clang.createIndex(0, 0);
    if (!index) {
        fs::remove(unitySource, iteratorError);
        logLines.push_back("ERROR: clang_createIndex failed.");
        logLines.push_back("Verdict: SETUP FAILED (no SDK files were changed).");
        WriteAstGenerationLog(sdkRoot / "SDKAstGeneration.log", logLines);
        delete data;
        return 1;
    }
    SdkLog("[SDK] Native libclang AST generation started for %s (1 unity translation unit, %zu source files, %zu include directories)", gameLabel.c_str(), sourceFiles.size(), includeDirectories.size());
    logLines.push_back("Unity source files: " + std::to_string(sourceFiles.size()));
    logLines.push_back("Toolchain include directories: " + std::to_string(includeDirectories.size()));
    std::vector<AstTranslationUnitResult> results;
    results.reserve(1);
    results.push_back(ParseAstTranslationUnit(clang, index, sdkRoot, unitySource, arguments));
    clang.disposeIndex(index);
    fs::remove(unitySource, iteratorError);

    size_t errorCount = 0;
    unsigned long long cursorCount = 0;
    for (const AstTranslationUnitResult& result : results) {
        cursorCount += result.cursorCount;
        errorCount += result.errors.size();
        logLines.insert(logLines.end(), result.errors.begin(), result.errors.end());
    }
    if (errorCount != 0) {
        logLines.push_back("Verdict: FAILED (raw SDK contains libclang errors; no files were changed).");
        WriteAstGenerationLog(sdkRoot / "SDKAstGeneration.log", logLines);
        SdkLog("[SDK] Native libclang AST generation failed for %s (%zu errors). Log: %s\\SDKAstGeneration.log", gameLabel.c_str(), errorCount, data->sdkPath.c_str());
    } else {
        WriteAstModel(sdkRoot, results);
        logLines.push_back("Parsed: 1 unity translation unit containing " + std::to_string(sourceFiles.size()) + " generated C++ source files");
        logLines.push_back("AST cursors visited: " + std::to_string(cursorCount));
        logLines.push_back("Verdict: PASSED (native libclang parsed and verified the generated SDK).");
        WriteAstGenerationLog(sdkRoot / "SDKAstGeneration.log", logLines);
        SdkLog("[SDK] Native libclang AST generation finished cleanly for %s. Log: %s\\SDKAstGeneration.log", gameLabel.c_str(), data->sdkPath.c_str());
    }
    delete data;
    return 0;
}
}

void StartSDKGenerationThread(const std::string& sdkPath,
                              const std::string& gameName,
                              const std::string& libclangPath) {
    auto* data = new SDKGenerationThreadData{ sdkPath, gameName, libclangPath };
    HANDLE thread = CreateThread(nullptr, 0, SDKGenerationThreadProc, data, 0, nullptr);
    if (!thread) {
        SdkLog("[SDK] WARNING: Failed to spawn libclang generation thread. Error: %lu", GetLastError());
        delete data;
        return;
    }
    CloseHandle(thread);
}

}
