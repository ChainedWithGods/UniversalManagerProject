#include "SDKGenerator_Impl.h"
#include "SDKGenerator_LayoutRules.h"
#include <windows.h>
namespace SDKGenerator {

void SdkLog(const char* fmt, ...) {
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    EngineDiscovery::WriteSdkLogLine(buf);
}

    bool IsCppKeyword(const std::string& name) {
        static const char* keywords[] = {

            "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
            "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
            "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
            "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
            "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
            "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
            "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
            "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected",
            "public", "register", "reinterpret_cast", "requires", "return", "short",
            "signed", "sizeof", "static", "static_assert", "static_cast", "struct",
            "switch", "template", "this", "thread_local", "throw", "true", "try",
            "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual",
            "void", "volatile", "wchar_t", "while", "xor", "xor_eq",

            "__cdecl", "__stdcall", "__fastcall", "__thiscall", "__vectorcall",
            "__forceinline", "__inline", "__declspec", "__attribute__",
            "__int8", "__int16", "__int32", "__int64", "__int128",
            "__based", "__except", "__finally", "__try", "__leave",
            "__asm", "__assume", "__uuidof", "__super", "__interface",
            "__sealed", "__abstract", "__event", "__hook", "__unhook", "__raise",
            "__ptr32", "__ptr64", "__sptr", "__uptr", "__unaligned",
            "__w64", "__clrcall", "__pragma",

            "TRUE", "FALSE", "NULL",
            "IN", "OUT", "OPTIONAL", "CONST",
            "NEAR", "FAR", "near", "far",
            "VOID", "BOOL", "BYTE", "WORD", "DWORD", "QWORD",
            "FLOAT", "DOUBLE", "CHAR", "WCHAR", "SHORT", "LONG", "UINT",
            "UCHAR", "USHORT", "ULONG", "LONGLONG", "ULONGLONG",
            "DWORD32", "DWORD64", "DWORD_PTR", "LONG_PTR", "ULONG_PTR",
            "INT_PTR", "UINT_PTR", "SIZE_T", "SSIZE_T",
            "HANDLE", "HWND", "HDC", "HMODULE", "HINSTANCE", "HRESULT",
            "LPVOID", "LPCSTR", "LPSTR", "LPCWSTR", "LPWSTR",
            "CALLBACK", "WINAPI", "APIENTRY", "APIPRIVATE",
            "PURE", "THIS", "THIS_",
            "STRICT", "INTERFACE", "interface",
            "DELETE",
            "IGNORE",
            "INFINITE",
            "OVERFLOW",
            "UNDERFLOW",
            "TRANSPARENT",

            "ERROR",
            "OPAQUE",
            "ABSOLUTE",
            "RELATIVE",
            "WHITE",
            "BLACK",

            "DOMAIN",
            "SING", "TLOSS", "PLOSS", "HUGE",

            "GetMessage", "PeekMessage", "SendMessage", "PostMessage",
            "DispatchMessage", "TranslateMessage",
            "GetObject", "DeleteObject",
            "GetCurrentTime", "Yield",
            "FindText", "ReplaceText",
            "LoadBitmap", "LoadCursor", "LoadIcon", "LoadImage",
            "LoadString", "LoadMenu",
            "GetClassName", "SetClassName",
            "CreateWindow", "CreateWindowEx",
            "CreateDialog", "CreateDialogParam",
            "RegisterClass", "UnregisterClass",
            "GetWindowText", "SetWindowText",
            "GetWindowLong", "SetWindowLong",
            "GetWindowLongPtr", "SetWindowLongPtr",
            "CallWindowProc",
            "DrawText", "DrawTextEx",
            "GetTextExtentPoint", "GetCharWidth",
            "FindResource", "LoadResource",
            "EnumResourceNames", "EnumResourceTypes",
            "OutputDebugString",
            "GetProp", "SetProp", "RemoveProp",
            "GetComputerName", "SetComputerName",
            "GetUserName",
            "GetStartupInfo",
            "CreateFile", "DeleteFile", "CopyFile", "MoveFile",
            "GetTempPath", "GetTempFileName",
            "CreateProcess",
            "OpenMutex", "CreateMutex",
            "OpenEvent", "CreateEvent",
            "OpenSemaphore", "CreateSemaphore",
            "GetModuleHandle", "GetModuleFileName",
            "GetProcAddress",
            "PlaySound", "MessageBox", "MessageBeep",
            "FormatMessage",
            "RegOpenKey", "RegCreateKey", "RegDeleteKey",
            "RegQueryValue", "RegSetValue",

            "TEXT", "UI", "DOMAIN", "DOMAIN_",
            "min", "max",
            "small",
            "SIZE_MAX", "INT_MAX", "INT_MIN", "UINT_MAX",

            nullptr
        };
        for (int i = 0; keywords[i]; ++i) {
            if (name == keywords[i]) return true;
        }
        return false;
    }

    std::string CleanClassName(std::string name) {
        if (IsCorePredefinedClass(name)) {
            return GetCorePredefinedCanonicalName(name);
        }

        std::string out;
        out.reserve(name.size());
        bool pendingUnderscore = false;
        for (unsigned char c : name) {
            const bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                               (c >= '0' && c <= '9') || c == '_';
            if (valid) {
                out += static_cast<char>(c);
                pendingUnderscore = false;
            } else if (!pendingUnderscore) {
                out += '_';
                pendingUnderscore = true;
            }
        }
        if (out.empty()) out = "_";
        if (out[0] >= '0' && out[0] <= '9') out.insert(out.begin(), '_');
        if (IsCppKeyword(out)) out += '_';
        return out;
    }

    std::string MakeEnumCppName(const std::string& cleanName) {
        if (cleanName.size() >= 2 && cleanName[0] == 'E' &&
            cleanName[1] >= 'A' && cleanName[1] <= 'Z') {
            return cleanName;
        }
        return "E" + cleanName;
    }

    std::string MakeUniqueName(const std::string& wanted, std::set<std::string>& used) {
        if (wanted.empty()) return wanted;
        std::string candidate = wanted;
        int suffix = 2;
        while (!used.insert(candidate).second) {
            candidate = wanted + "_" + std::to_string(suffix++);
        }
        return candidate;
    }

    bool IsReservedWindowsName(const std::string& name) {
        static const char* reserved[] = {
            "CON", "PRN", "AUX", "NUL",
            "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
            "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
            nullptr
        };
        for (int i = 0; reserved[i]; ++i) {
            const char* r = reserved[i];
            size_t len = 0;
            while (r[len]) ++len;
            if (name.length() == len) {
                bool eq = true;
                for (size_t j = 0; j < len; ++j) {
                    if ((name[j] | 0x20) != r[j]) { eq = false; break; }
                }
                if (eq) return true;
            }
        }
        return false;
    }

    std::string ResolvePackageName(const std::string& rawName,
                                          std::map<std::string, int>& collisionCounter,
                                          std::map<std::string, std::string>& rawToUnique) {
        auto it = rawToUnique.find(rawName);
        if (it != rawToUnique.end()) return it->second;

        std::string cleanName = rawName;
        if (cleanName.find("/Script/") == 0) cleanName = cleanName.substr(8);
        else if (cleanName.find("/Game/") == 0) cleanName = cleanName.substr(6);
        cleanName = CleanClassName(cleanName);

        if (IsReservedWindowsName(cleanName)) cleanName += "_pkg";
        if (cleanName.empty()) cleanName = "UnknownPackage";

        int& count = collisionCounter[cleanName];
        std::string uniqueName = cleanName;
        if (count > 0) {
            uniqueName = cleanName + "_" + std::to_string(count);
        }
        count++;
        rawToUnique[rawName] = uniqueName;
        return uniqueName;
    }

    uint64_t HashBytesFNV1a(uint64_t hash, const void* data, size_t size) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            hash ^= bytes[i];
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    uint64_t HashStringFNV1a(uint64_t hash, const std::string& value) {
        hash = HashBytesFNV1a(hash, value.data(), value.size());
        const uint8_t separator = 0xFF;
        return HashBytesFNV1a(hash, &separator, sizeof(separator));
    }

    uint64_t HashUInt64FNV1a(uint64_t hash, uint64_t value) {
        return HashBytesFNV1a(hash, &value, sizeof(value));
    }

    std::string FormatHashHex(uint64_t hash) {
        std::ostringstream out;
        out << "0x" << std::hex << std::uppercase << hash;
        return out.str();
    }

    std::wstring ToWidePath(const std::string& narrowPath) {
        if (narrowPath.empty()) return {};

        int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, narrowPath.c_str(), -1, nullptr, 0);
        UINT codePage = CP_UTF8;
        DWORD flags = MB_ERR_INVALID_CHARS;
        if (wideLength <= 0) {
            codePage = CP_ACP;
            flags = 0;
            wideLength = MultiByteToWideChar(codePage, flags, narrowPath.c_str(), -1, nullptr, 0);
        }
        if (wideLength <= 0) {
            return {};
        }

        std::wstring widePath(static_cast<size_t>(wideLength - 1), L'\0');
        MultiByteToWideChar(codePage, flags, narrowPath.c_str(), -1, widePath.data(), wideLength);
        return widePath;
    }

    void AppendTag(std::string& output, const char* tag) {
        if (!output.empty()) output += "|";
        output += tag;
    }

    std::string DescribePropertyFlags(uint64_t flags) {
        std::string tags;
        if (flags & CPF_Edit) AppendTag(tags, "Edit");
        if (flags & CPF_BlueprintVisible) AppendTag(tags, "BlueprintVisible");
        if (flags & CPF_BlueprintReadOnly) AppendTag(tags, "BlueprintReadOnly");
        if (flags & CPF_Net) AppendTag(tags, "Net");
        if (flags & CPF_EditFixedSize) AppendTag(tags, "EditFixedSize");
        if (flags & CPF_ZeroConstructor) AppendTag(tags, "ZeroConstructor");
        if (flags & CPF_DisableEditOnTemplate) AppendTag(tags, "DisableEditOnTemplate");
        if (flags & CPF_ExportObject) AppendTag(tags, "ExportObject");
        if (flags & CPF_NonNullable) AppendTag(tags, "NonNullable");
        if (flags & CPF_Transient) AppendTag(tags, "Transient");
        if (flags & CPF_Config) AppendTag(tags, "Config");
        if (flags & CPF_RequiredParm) AppendTag(tags, "RequiredParm");
        if (flags & CPF_DisableEditOnInstance) AppendTag(tags, "DisableEditOnInstance");
        if (flags & CPF_EditConst) AppendTag(tags, "EditConst");
        if (flags & CPF_GlobalConfig) AppendTag(tags, "GlobalConfig");
        if (flags & CPF_InstancedReference) AppendTag(tags, "Instanced");
        if (flags & CPF_DuplicateTransient) AppendTag(tags, "DuplicateTransient");
        if (flags & CPF_SaveGame) AppendTag(tags, "SaveGame");
        if (flags & CPF_NoClear) AppendTag(tags, "NoClear");
        if (flags & CPF_BlueprintAssignable) AppendTag(tags, "BlueprintAssignable");
        if (flags & CPF_Deprecated) AppendTag(tags, "Deprecated");
        if (flags & CPF_IsPlainOldData) AppendTag(tags, "IsPlainOldData");
        if (flags & CPF_RepSkip) AppendTag(tags, "RepSkip");
        if (flags & CPF_RepNotify) AppendTag(tags, "RepNotify");
        if (flags & CPF_Interp) AppendTag(tags, "Interp");
        if (flags & CPF_NonTransactional) AppendTag(tags, "NonTransactional");
        if (flags & CPF_EditorOnly) AppendTag(tags, "EditorOnly");
        if (flags & CPF_NoDestructor) AppendTag(tags, "NoDestructor");
        if (flags & CPF_AutoWeak) AppendTag(tags, "AutoWeak");
        if (flags & CPF_ContainsInstancedReference) AppendTag(tags, "ContainsInstancedReference");
        if (flags & CPF_AssetRegistrySearchable) AppendTag(tags, "AssetRegistrySearchable");
        if (flags & CPF_SimpleDisplay) AppendTag(tags, "SimpleDisplay");
        if (flags & CPF_AdvancedDisplay) AppendTag(tags, "AdvancedDisplay");
        if (flags & CPF_BlueprintCallable) AppendTag(tags, "BlueprintCallable");
        if (flags & CPF_BlueprintAuthorityOnly) AppendTag(tags, "BlueprintAuthorityOnly");
        if (flags & CPF_TextExportTransient) AppendTag(tags, "TextExportTransient");
        if (flags & CPF_NonPIEDuplicateTransient) AppendTag(tags, "NonPIEDuplicateTransient");
        if (flags & CPF_ExposeOnSpawn) AppendTag(tags, "ExposeOnSpawn");
        if (flags & CPF_PersistentInstance) AppendTag(tags, "PersistentInstance");
        if (flags & CPF_UObjectWrapper) AppendTag(tags, "UObjectWrapper");
        if (flags & CPF_HasGetValueTypeHash) AppendTag(tags, "HasGetValueTypeHash");
        if (flags & CPF_NativeAccessSpecifierPublic) AppendTag(tags, "Public");
        if (flags & CPF_NativeAccessSpecifierProtected) AppendTag(tags, "Protected");
        if (flags & CPF_NativeAccessSpecifierPrivate) AppendTag(tags, "Private");
        if (flags & CPF_SkipSerialization) AppendTag(tags, "SkipSerialization");
        if (flags & CPF_Parm) AppendTag(tags, "Parm");
        if (flags & CPF_OutParm) AppendTag(tags, "Out");
        if (flags & CPF_ReturnParm) AppendTag(tags, "Return");
        if (flags & CPF_ReferenceParm) AppendTag(tags, "Ref");
        if (flags & CPF_ConstParm) AppendTag(tags, "Const");
        return tags.empty() ? "None" : tags;
    }

    std::string DescribeFunctionFlags(uint32_t flags) {
        std::string tags;
        if (flags & FUNC_Final) AppendTag(tags, "Final");
        if (flags & FUNC_RequiredAPI) AppendTag(tags, "RequiredAPI");
        if (flags & FUNC_BlueprintAuthorityOnly) AppendTag(tags, "BlueprintAuthorityOnly");
        if (flags & FUNC_BlueprintCosmetic) AppendTag(tags, "BlueprintCosmetic");
        if (flags & FUNC_Net) AppendTag(tags, "Net");
        if (flags & FUNC_NetReliable) AppendTag(tags, "Reliable");
        if (flags & FUNC_Native) AppendTag(tags, "Native");
        if (flags & FUNC_Event) AppendTag(tags, "Event");
        if (flags & FUNC_Static) AppendTag(tags, "Static");
        if (flags & FUNC_NetMulticast) AppendTag(tags, "NetMulticast");
        if (flags & FUNC_MulticastDelegate) AppendTag(tags, "MulticastDelegate");
        if (flags & FUNC_Public) AppendTag(tags, "Public");
        if (flags & FUNC_Private) AppendTag(tags, "Private");
        if (flags & FUNC_Protected) AppendTag(tags, "Protected");
        if (flags & FUNC_Delegate) AppendTag(tags, "Delegate");
        if (flags & FUNC_NetServer) AppendTag(tags, "Server");
        if (flags & FUNC_HasOutParms) AppendTag(tags, "HasOutParms");
        if (flags & FUNC_HasDefaults) AppendTag(tags, "HasDefaults");
        if (flags & FUNC_NetClient) AppendTag(tags, "Client");
        if (flags & FUNC_BlueprintCallable) AppendTag(tags, "BlueprintCallable");
        if (flags & FUNC_BlueprintEvent) AppendTag(tags, "BlueprintEvent");
        if (flags & FUNC_BlueprintPure) AppendTag(tags, "BlueprintPure");
        if (flags & FUNC_Const) AppendTag(tags, "Const");
        return tags.empty() ? "None" : tags;
    }

    bool WriteFile(const std::string& outputDir, const std::string& fileName, const std::string& content) {
        std::string fullPath = outputDir + "\\" + fileName;
        const std::wstring wideFullPath = ToWidePath(fullPath);
        if (wideFullPath.empty()) {
            SdkLog("[SDK] ERROR: Failed to convert output path to UTF-16: %s", fullPath.c_str());
            return false;
        }

        HANDLE hFile = CreateFileW(
            wideFullPath.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        if (hFile == INVALID_HANDLE_VALUE) {
            std::error_code ec;
            fs::create_directories(fs::path(wideFullPath).parent_path(), ec);
            hFile = CreateFileW(
                wideFullPath.c_str(),
                GENERIC_WRITE,
                FILE_SHARE_READ,
                NULL,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );
        }
        if (hFile == INVALID_HANDLE_VALUE) {
            SdkLog("[SDK] ERROR: Failed to open file for writing: %s (len=%zu)", fullPath.c_str(), fullPath.length());
            return false;
        }

        DWORD bytesWritten = 0;
        const BOOL success = ::WriteFile(hFile, content.data(), static_cast<DWORD>(content.size()), &bytesWritten, NULL);
        CloseHandle(hFile);
        if (!success || bytesWritten != content.size()) {
            SdkLog("[SDK] ERROR: Failed to write file: %s (err=%lu)", fullPath.c_str(), GetLastError());
            return false;
        }
        return true;
    }

    void* ReadPtrAt(void* base, int32_t offset) {
        if (!base || offset < 0) return nullptr;
        return SafeMem_ReadPtr((uint8_t*)base + offset);
    }

    GenerationCaches& GetGenerationCaches() {
        static GenerationCaches caches;
        return caches;
    }

    std::string GetCachedObjectName(const EngineState& state, UObject* obj) {
        if (!obj) return {};
        GenerationCaches& caches = GetGenerationCaches();
        auto it = caches.ObjectNames.find(obj);
        if (it != caches.ObjectNames.end()) return it->second;

        std::string value = EngineDiscovery::GetObjectNameStr(state, obj);
        caches.ObjectNames.emplace(obj, value);
        return value;
    }

    std::string GetCachedCleanObjectName(const EngineState& state, UObject* obj) {
        if (!obj) return {};
        GenerationCaches& caches = GetGenerationCaches();
        auto it = caches.CleanObjectNames.find(obj);
        if (it != caches.CleanObjectNames.end()) return it->second;

        std::string value = CleanClassName(GetCachedObjectName(state, obj));
        caches.CleanObjectNames.emplace(obj, value);
        return value;
    }

    std::string GetCachedFullName(const EngineState& state, UObject* obj) {
        if (!obj) return {};
        GenerationCaches& caches = GetGenerationCaches();
        auto it = caches.FullNames.find(obj);
        if (it != caches.FullNames.end()) return it->second;

        std::string value = EngineDiscovery::BuildFullName(state, obj);
        caches.FullNames.emplace(obj, value);
        return value;
    }

    std::string GetCachedFFieldName(const EngineState& state, void* field) {
        if (!field) return {};
        GenerationCaches& caches = GetGenerationCaches();
        auto it = caches.FieldNames.find(field);
        if (it != caches.FieldNames.end()) return it->second;

        std::string value = EngineDiscovery::GetFFieldName(state, field);
        caches.FieldNames.emplace(field, value);
        return value;
    }

    std::string GetCachedFFieldTypeName(const EngineState& state, void* field) {
        if (!field) return {};
        GenerationCaches& caches = GetGenerationCaches();
        auto it = caches.FieldTypeNames.find(field);
        if (it != caches.FieldTypeNames.end()) return it->second;

        std::string value = EngineDiscovery::GetFFieldTypeName(state, field);
        caches.FieldTypeNames.emplace(field, value);
        return value;
    }

    std::string DescribePropertyFlagsCached(uint64_t flags) {
        GenerationCaches& caches = GetGenerationCaches();
        auto it = caches.PropertyFlagsText.find(flags);
        if (it != caches.PropertyFlagsText.end()) return it->second;

        std::string value = DescribePropertyFlags(flags);
        caches.PropertyFlagsText.emplace(flags, value);
        return value;
    }

    std::string DescribeFunctionFlagsCached(uint32_t flags) {
        GenerationCaches& caches = GetGenerationCaches();
        auto it = caches.FunctionFlagsText.find(flags);
        if (it != caches.FunctionFlagsText.end()) return it->second;

        std::string value = DescribeFunctionFlags(flags);
        caches.FunctionFlagsText.emplace(flags, value);
        return value;
    }

    UObject* GetOutermost(const EngineState& state, UObject* obj) {
        UObject* outermost = obj;
        UObject* outer = EngineDiscovery::GetObjectOuter(state, obj);
        while (outer) {
            outermost = outer;
            outer = EngineDiscovery::GetObjectOuter(state, outer);
        }
        return outermost;
    }

    std::string GetPackageName(const EngineState& state, UObject* obj,
                                      std::map<std::string, int>& collisionCounter,
                                      std::map<std::string, std::string>& rawToUnique) {
        if (!obj) return "UnknownPackage";
        GenerationCaches& caches = GetGenerationCaches();
        auto cacheIt = caches.PackageNames.find(obj);
        if (cacheIt != caches.PackageNames.end()) return cacheIt->second;

        UObject* pkg = GetOutermost(state, obj);
        if (!pkg) return "UnknownPackage";
        std::string rawName = GetCachedObjectName(state, pkg);
        std::string resolved = ResolvePackageName(rawName, collisionCounter, rawToUnique);
        caches.PackageNames.emplace(obj, resolved);
        return resolved;
    }

    TypeDependencyContext& GetTypeDependencyContext() {
        static TypeDependencyContext ctx;
        return ctx;
    }

    void TypeDependencyContext::Reset(const EngineState& s,
                                      std::map<std::string, int>& cc,
                                      std::map<std::string, std::string>& ru,
                                      const std::string& self) {
        state = &s;
        collisionCounter = &cc;
        rawToUnique = &ru;
        selfPackage = self;
        deps.clear();
    }

    void TypeDependencyContext::Finish(std::map<std::string, std::set<std::string>>& packageDependencies) {
        if (state && !selfPackage.empty()) {
            std::set<std::string>& target = packageDependencies[selfPackage];
            for (const std::string& dep : deps) {
                if (dep != selfPackage) target.insert(dep);
            }
        }
        state = nullptr;
        deps.clear();
    }

    void GenerationCaches::Clear() {
        ObjectNames.clear();
        CleanObjectNames.clear();
        FullNames.clear();
        PackageNames.clear();
        FieldNames.clear();
        FieldTypeNames.clear();
        PropertyCppTypes.clear();
        IsActorDerived.clear();
        IsScriptStruct.clear();
        IsClass.clear();
        IsFunction.clear();
        IsEnum.clear();
        PropertyFlagsText.clear();
        FunctionFlagsText.clear();
        EnumUnderlyingBytes.clear();
    }

    void NoteValueTypeDependency(const EngineState& state, const void* referencedObj) {
        TypeDependencyContext& ctx = GetTypeDependencyContext();
        if (!ctx.state || !referencedObj || !ctx.collisionCounter || !ctx.rawToUnique) return;
        const std::string pkg = GetPackageName(state, (UObject*)referencedObj,
                                               *ctx.collisionCounter, *ctx.rawToUnique);
        if (!pkg.empty() && pkg != ctx.selfPackage) ctx.deps.insert(pkg);
    }

    bool IsActorDerived(const EngineState& state, void* obj) {
        if (!obj) return false;
        if (state.AActorClass && obj == state.AActorClass) return true;
        GenerationCaches& caches = GetGenerationCaches();
        auto cached = caches.IsActorDerived.find(obj);
        if (cached != caches.IsActorDerived.end()) return cached->second;

        UStruct* scan = (UStruct*)obj;
        bool isActor = false;
        const int32_t superOff = state.UStruct_SuperOff;
        for (int depth = 0; depth < 20 && scan; depth++) {
            if (state.AActorClass && scan == state.AActorClass) {
                isActor = true;
                break;
            }
            if (GetCachedObjectName(state, (UObject*)scan) == "Actor") {
                isActor = true;
                break;
            }
            if (superOff >= 0) {
                scan = *(UStruct**)((uint8_t*)scan + superOff);
            } else {
                scan = EngineDiscovery::GetSuperStruct(state, scan);
            }
        }
        caches.IsActorDerived.emplace(obj, isActor);
        return isActor;
    }

    bool IsScriptStruct(const EngineState& state, UClass* metaCls) {
        if (!metaCls) return false;
        GenerationCaches& caches = GetGenerationCaches();
        auto cached = caches.IsScriptStruct.find(metaCls);
        if (cached != caches.IsScriptStruct.end()) return cached->second;

        UStruct* scan = (UStruct*)metaCls;
        bool result = false;
        const int32_t superOff = state.UStruct_SuperOff;
        for (int depth = 0; depth < 10 && scan; depth++) {
            if (GetCachedObjectName(state, (UObject*)scan) == "ScriptStruct") {
                result = true;
                break;
            }
            if (superOff >= 0) {
                scan = *(UStruct**)((uint8_t*)scan + superOff);
            } else {
                scan = EngineDiscovery::GetSuperStruct(state, scan);
            }
        }
        caches.IsScriptStruct.emplace(metaCls, result);
        return result;
    }

    bool IsEnum(const EngineState& state, UClass* metaCls) {
        if (!metaCls) return false;
        if (state.UEnumClass && metaCls == state.UEnumClass) return true;
        GenerationCaches& caches = GetGenerationCaches();
        auto cached = caches.IsEnum.find(metaCls);
        if (cached != caches.IsEnum.end()) return cached->second;

        UStruct* scan = (UStruct*)metaCls;
        bool result = false;
        const int32_t superOff = state.UStruct_SuperOff;
        for (int depth = 0; depth < 10 && scan; depth++) {
            if (state.UEnumClass && scan == state.UEnumClass) {
                result = true;
                break;
            }
            const std::string name = GetCachedObjectName(state, (UObject*)scan);
            if (name == "Enum" || name == "UserDefinedEnum") {
                result = true;
                break;
            }
            if (superOff >= 0) {
                scan = *(UStruct**)((uint8_t*)scan + superOff);
            } else {
                scan = EngineDiscovery::GetSuperStruct(state, scan);
            }
        }
        caches.IsEnum.emplace(metaCls, result);
        return result;
    }

    bool IsClass(const EngineState& state, UClass* metaCls) {
        if (!metaCls) return false;
        if (state.UClassClass && metaCls == state.UClassClass) return true;
        GenerationCaches& caches = GetGenerationCaches();
        auto cached = caches.IsClass.find(metaCls);
        if (cached != caches.IsClass.end()) return cached->second;

        UStruct* scan = (UStruct*)metaCls;
        bool result = false;
        const int32_t superOff = state.UStruct_SuperOff;
        for (int depth = 0; depth < 10 && scan; depth++) {
            if (state.UClassClass && scan == state.UClassClass) {
                result = true;
                break;
            }
            if (GetCachedObjectName(state, (UObject*)scan) == "Class") {
                result = true;
                break;
            }
            if (superOff >= 0) {
                scan = *(UStruct**)((uint8_t*)scan + superOff);
            } else {
                scan = EngineDiscovery::GetSuperStruct(state, scan);
            }
        }
        caches.IsClass.emplace(metaCls, result);
        return result;
    }

    bool IsFunction(const EngineState& state, UClass* metaCls) {
        if (!metaCls) return false;
        if (state.UFunctionClass && metaCls == state.UFunctionClass) return true;
        GenerationCaches& caches = GetGenerationCaches();
        auto cached = caches.IsFunction.find(metaCls);
        if (cached != caches.IsFunction.end()) return cached->second;

        UStruct* scan = (UStruct*)metaCls;
        bool result = false;
        const int32_t superOff = state.UStruct_SuperOff;
        for (int depth = 0; depth < 10 && scan; depth++) {
            if (state.UFunctionClass && scan == state.UFunctionClass) {
                result = true;
                break;
            }
            if (GetCachedObjectName(state, (UObject*)scan) == "Function") {
                result = true;
                break;
            }
            if (superOff >= 0) {
                scan = *(UStruct**)((uint8_t*)scan + superOff);
            } else {
                scan = EngineDiscovery::GetSuperStruct(state, scan);
            }
        }
        caches.IsFunction.emplace(metaCls, result);
        return result;
    }

    std::unordered_map<const void*, std::string>& GetUniqueTypeNameMap() {
        static std::unordered_map<const void*, std::string> m;
        return m;
    }
    std::unordered_map<std::string, int>& GetTypeNameUseCounts() {
        static std::unordered_map<std::string, int> m;
        return m;
    }
    std::string GetUniqueTypeCppName(const void* obj, const std::string& baseName) {
        auto& byObj = GetUniqueTypeNameMap();
        auto it = byObj.find(obj);
        if (it != byObj.end()) return it->second;
        auto& counts = GetTypeNameUseCounts();
        std::string unique = baseName;
        int suffix = ++counts[baseName];
        if (suffix > 1) {
            unique = baseName + "_" + std::to_string(suffix);

            while (counts.count(unique)) unique = baseName + "_" + std::to_string(++suffix);
            counts[unique] = 1;
        }
        byObj.emplace(obj, unique);
        return unique;
    }

    static int32_t ReadPropertyElementSizeOf(const EngineState& state, void* prop) {
        if (!prop || state.FProperty_ElementSizeOff < 0) return -1;
        int32_t size = 0;
        SafeMem_Read((uint8_t*)prop + state.FProperty_ElementSizeOff, &size, 4);
        return size;
    }

    std::string PropertyTypeToCpp(const EngineState& state, void* prop, const std::string& pType,
                                  int32_t propertyElementSize) {
        if (prop && propertyElementSize <= 0) {
            GenerationCaches& caches = GetGenerationCaches();
            auto it = caches.PropertyCppTypes.find(prop);
            if (it != caches.PropertyCppTypes.end()) return it->second;
        }

        std::string resolvedType;
        if (pType == "BoolProperty") resolvedType = "bool";
        else if (pType == "IntProperty" || pType == "Int32Property") resolvedType = "int32_t";
        else if (pType == "Int8Property") resolvedType = "int8_t";
        else if (pType == "Int16Property") resolvedType = "int16_t";
        else if (pType == "Int64Property") resolvedType = "int64_t";
        else if (pType == "UInt16Property") resolvedType = "uint16_t";
        else if (pType == "UInt32Property") resolvedType = "uint32_t";
        else if (pType == "UInt64Property") resolvedType = "uint64_t";
        else if (pType == "FloatProperty") resolvedType = "float";
        else if (pType == "DoubleProperty") resolvedType = "double";
        if (pType == "ByteProperty") {

            if (state.FByteProperty_EnumOff >= 0) {
                void* enumPtr = ReadPtrAt(prop, state.FByteProperty_EnumOff);
                if (enumPtr) {
                    std::string enumName = GetCachedObjectName(state, (UObject*)enumPtr);
                    if (!enumName.empty()) {
                        const std::string enumCppName =
                            GetUniqueTypeCppName(enumPtr, MakeEnumCppName(CleanClassName(enumName)));

                        if (propertyElementSize > 0) {
                            const std::string wrapped = FixedSizeEnumWrapper(
                                enumCppName, GetEnumUnderlyingBytesCached(state, enumPtr),
                                static_cast<uint32_t>(propertyElementSize));
                            if (!wrapped.empty()) {
                                resolvedType = wrapped;
                                NoteValueTypeDependency(state, enumPtr);
                            }
                        }

                        if (resolvedType.empty()) {
                            resolvedType = enumCppName;
                            NoteValueTypeDependency(state, enumPtr);
                        }
                    }
                }
            }
            if (resolvedType.empty()) resolvedType = "uint8_t";
        }
        else if (pType == "NameProperty") resolvedType = "struct FName";
        else if (pType == "StrProperty") resolvedType = "class FString";
        else if (pType == "Utf8StrProperty") resolvedType = "class FUtf8String";
        else if (pType == "TextProperty") resolvedType = "class FText";
        else if (pType == "OptionalProperty") {

            int32_t size = propertyElementSize;
            if (size <= 0) size = ReadPropertyElementSizeOf(state, prop);
            resolvedType = "SDK_OptionalBytes<" + std::to_string(size > 0 ? size : 1) + ">";
        }
        else if (pType == "ObjectProperty" || pType == "ObjectPtrProperty") {
            if (state.FObjectProperty_ClassOff >= 0) {
                void* classPtr = ReadPtrAt(prop, state.FObjectProperty_ClassOff);
                if (classPtr) {
                    std::string clsName = CleanClassName(GetCachedObjectName(state, (UObject*)classPtr));
                    if (!clsName.empty()) {
                        std::string pfx = IsActorDerived(state, classPtr) ? "A" : "U";
                        resolvedType = "class " + GetUniqueTypeCppName(classPtr, pfx + clsName) + "*";
                    }
                }
            }
            if (resolvedType.empty()) resolvedType = "class UObject*";
        }
        else if (pType == "ClassProperty" || pType == "ClassPtrProperty") {
            if (state.FClassProperty_MetaClassOff >= 0) {
                void* metaCls = ReadPtrAt(prop, state.FClassProperty_MetaClassOff);
                if (metaCls) {
                    std::string mcName = CleanClassName(GetCachedObjectName(state, (UObject*)metaCls));
                    if (!mcName.empty()) {
                        std::string pfx = IsActorDerived(state, metaCls) ? "A" : "U";
                        resolvedType = "TSubclassOf<class " + GetUniqueTypeCppName(metaCls, pfx + mcName) + ">";
                    }
                }
            }
            if (resolvedType.empty()) resolvedType = "class UClass*";
        }
        else if (pType == "SoftObjectProperty") resolvedType = "TSoftObjectPtr<UObject>";
        else if (pType == "SoftClassProperty") resolvedType = "TSoftClassPtr<UObject>";
        else if (pType == "WeakObjectProperty") resolvedType = "TWeakObjectPtr<UObject>";
        else if (pType == "LazyObjectProperty") resolvedType = "TLazyObjectPtr<UObject>";
        else if (pType == "InterfaceProperty") resolvedType = "FScriptInterface";
        else if (pType == "DelegateProperty") resolvedType = "FScriptDelegate";
        else if (pType == "MulticastDelegateProperty" || pType == "MulticastInlineDelegateProperty")
            resolvedType = "FMulticastScriptDelegate";
        else if (pType == "MulticastSparseDelegateProperty")
            resolvedType = "FMulticastSparseDelegateProperty_";
        else if (pType == "ArrayProperty") {
            if (state.FArrayProperty_InnerOff >= 0) {
                void* inner = ReadPtrAt(prop, state.FArrayProperty_InnerOff);
                if (inner) {
                    std::string innerType = GetCachedFFieldTypeName(state, inner);
                    resolvedType = "TArray<" + PropertyTypeToCpp(state, inner, innerType,
                                                               ReadPropertyElementSizeOf(state, inner)) + ">";
                }
            }
            if (resolvedType.empty()) resolvedType = "TArray<uint8_t>";
        }
        else if (pType == "SetProperty") {
            if (state.FSetProperty_ElementPropOff >= 0) {
                void* elem = ReadPtrAt(prop, state.FSetProperty_ElementPropOff);
                if (elem) {
                    std::string elemType = GetCachedFFieldTypeName(state, elem);
                    resolvedType = "TSet<" + PropertyTypeToCpp(state, elem, elemType,
                                                              ReadPropertyElementSizeOf(state, elem)) + ">";
                }
            }
            if (resolvedType.empty()) resolvedType = "TSet<uint8_t>";
        }
        else if (pType == "MapProperty") {
            if (state.FMapProperty_KeyPropOff >= 0 && state.FMapProperty_ValuePropOff >= 0) {
                void* key = ReadPtrAt(prop, state.FMapProperty_KeyPropOff);
                void* val = ReadPtrAt(prop, state.FMapProperty_ValuePropOff);
                if (key && val) {
                    std::string keyType = GetCachedFFieldTypeName(state, key);
                    std::string valType = GetCachedFFieldTypeName(state, val);
                    resolvedType = "TMap<" + PropertyTypeToCpp(state, key, keyType, ReadPropertyElementSizeOf(state, key))
                        + ", " + PropertyTypeToCpp(state, val, valType, ReadPropertyElementSizeOf(state, val)) + ">";
                }
            }
            if (resolvedType.empty()) resolvedType = "TMap<uint8_t, uint8_t>";
        }
        else if (pType == "StructProperty") {
            if (state.FStructProperty_StructOff >= 0) {
                void* structPtr = ReadPtrAt(prop, state.FStructProperty_StructOff);
                if (structPtr) {
                    std::string sName = CleanClassName(GetCachedObjectName(state, (UObject*)structPtr));
                    if (!sName.empty()) {
                        resolvedType = "struct " + GetUniqueTypeCppName(structPtr, "F" + sName);
                        NoteValueTypeDependency(state, structPtr);
                    }
                }
            }
            if (resolvedType.empty()) {

                int elemSize = 0;
                const int sizeOff = state.FProperty_ElementSizeOff >= 0 ? state.FProperty_ElementSizeOff : 0x3C;
                SafeMem_Read((uint8_t*)prop + sizeOff, &elemSize, 4);
                if (elemSize <= 0) elemSize = 1;
                std::ostringstream fallback;
                fallback << "FUnknownStruct<0x" << std::hex << elemSize << ">";
                resolvedType = fallback.str();
            }
        }
        else if (pType == "EnumProperty") {
            if (state.FEnumProperty_EnumOff >= 0) {
                void* enumPtr = ReadPtrAt(prop, state.FEnumProperty_EnumOff);
                if (enumPtr) {
                    std::string eName = CleanClassName(GetCachedObjectName(state, (UObject*)enumPtr));
                    if (!eName.empty()) {
                        const std::string enumCppName = GetUniqueTypeCppName(enumPtr, MakeEnumCppName(eName));

                        if (propertyElementSize > 0) {
                            const std::string wrapped = FixedSizeEnumWrapper(
                                enumCppName, GetEnumUnderlyingBytesCached(state, enumPtr),
                                static_cast<uint32_t>(propertyElementSize));
                            if (!wrapped.empty()) resolvedType = wrapped;
                        }

                        if (resolvedType.empty()) resolvedType = enumCppName;
                        NoteValueTypeDependency(state, enumPtr);

                        if (propertyElementSize > 0) {
                            int32_t& observed = GetGenerationCaches().EnumPropertyBytes[enumPtr];
                            if (propertyElementSize > observed) observed = propertyElementSize;
                        }
                    }
                }
            }
            if (resolvedType.empty()) resolvedType = "uint8_t";
        }
        else if (pType == "FieldPathProperty") resolvedType = "FFieldPath";

        if (resolvedType.empty()) resolvedType = "uint8_t";
        if (prop && propertyElementSize <= 0) {
            GetGenerationCaches().PropertyCppTypes.emplace(prop, resolvedType);
        }
        return resolvedType;
    }

}
