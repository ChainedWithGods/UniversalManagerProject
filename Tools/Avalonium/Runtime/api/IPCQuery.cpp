#include "IPCQuery.h"
#include "UEInternalAPI.h"
#include "../core/EngineDiscovery.h"
#include "../core/PatternScanner.h"
#include "../core/ObjectIndex.h"

extern "C" {
#include "../utils/SafeMemory.h"
}

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>
#include <psapi.h>

extern EngineState g_State;

#pragma comment(lib, "psapi.lib")

namespace IPCQuery {

static std::string EscapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04X", c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    return out;
}

static std::string PtrStr(uintptr_t v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)v);
    return buf;
}

static std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static bool ParseU64(const std::string& s, uint64_t& out) {
    std::string t = Trim(s);
    if (t.empty()) return false;
    if (t.size() > 2 && (t[0] == '0') && (t[1] == 'x' || t[1] == 'X')) {
        return sscanf_s(t.c_str() + 2, "%llx", &out) == 1;
    }
    return sscanf_s(t.c_str(), "%llu", &out) == 1;
}

static bool ParsePtr(const std::string& s, uint64_t& out) {
    std::string t = Trim(s);
    if (t.empty()) return false;
    if (t.size() > 2 && (t[0] == '0') && (t[1] == 'x' || t[1] == 'X')) {
        return sscanf_s(t.c_str() + 2, "%llx", &out) == 1;
    }
    return sscanf_s(t.c_str(), "%llx", &out) == 1;
}

static std::string JsonUnescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\' || i + 1 >= s.size()) { out += s[i]; continue; }
        char n = s[++i];
        switch (n) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case '"': out += '"';  break;
            case '\\': out += '\\'; break;
            case '/': out += '/';  break;
            default: out += n;
        }
    }
    return out;
}

static void ParseFlatJson(const std::string& text, std::map<std::string, std::string>& out) {
    size_t i = 0;
    const size_t n = text.size();

    auto skipWs = [&]() { while (i < n && isspace((unsigned char)text[i])) ++i; };

    auto readString = [&](std::string& dst) -> bool {
        if (i >= n || text[i] != '"') return false;
        ++i;
        std::string raw;
        while (i < n && text[i] != '"') {
            if (text[i] == '\\' && i + 1 < n) { raw += text[i]; raw += text[i + 1]; i += 2; continue; }
            raw += text[i++];
        }
        if (i >= n) return false;
        ++i;
        dst = JsonUnescape(raw);
        return true;
    };

    skipWs();
    if (i >= n || text[i] != '{') return;
    ++i;

    while (i < n) {
        skipWs();
        if (i < n && text[i] == '}') break;
        if (i < n && text[i] == ',') { ++i; continue; }

        std::string key;
        if (!readString(key)) break;

        skipWs();
        if (i >= n || text[i] != ':') break;
        ++i;
        skipWs();

        std::string value;
        if (i < n && text[i] == '"') {
            if (!readString(value)) break;
        } else if (i < n && text[i] == '[') {

            int depth = 0;
            std::string collected;
            while (i < n) {
                char c = text[i];
                if (c == '[') ++depth;
                else if (c == ']') { --depth; if (depth == 0) { ++i; break; } }
                else if (c == '"') { ++i; continue; }
                if (depth > 0 && c != '[') collected += c;
                ++i;
            }
            value = collected;
        } else {
            while (i < n && text[i] != ',' && text[i] != '}') value += text[i++];
            value = Trim(value);
        }

        out[Lower(key)] = value;
    }
}

static std::map<std::string, std::string> ParseArgs(const std::string& args) {
    std::map<std::string, std::string> kv;
    std::string text = Trim(args);
    if (text.empty()) return kv;

    if (text[0] == '{') {
        ParseFlatJson(text, kv);
        return kv;
    }

    size_t i = 0;
    const size_t n = text.size();
    while (i < n) {
        while (i < n && isspace((unsigned char)text[i])) ++i;
        if (i >= n) break;

        std::string key;
        while (i < n && text[i] != '=' && !isspace((unsigned char)text[i])) key += text[i++];
        if (i >= n || text[i] != '=') { break; }
        ++i;

        std::string value;
        if (i < n && text[i] == '"') {
            ++i;
            while (i < n && text[i] != '"') {
                if (text[i] == '\\' && i + 1 < n) { value += text[++i]; }
                else value += text[i];
                ++i;
            }
            if (i < n) ++i;
        } else {
            while (i < n && !isspace((unsigned char)text[i])) value += text[i++];
        }
        kv[Lower(key)] = value;
    }
    return kv;
}

static std::string GetArg(const std::map<std::string, std::string>& kv,
                          const char* key, const std::string& fallback = "") {
    auto it = kv.find(key);
    return it == kv.end() ? fallback : it->second;
}

static int GetIntArg(const std::map<std::string, std::string>& kv,
                     const char* key, int fallback, int lo, int hi) {
    auto it = kv.find(key);
    if (it == kv.end() || it->second.empty()) return fallback;
    long long v = _strtoi64(it->second.c_str(), nullptr, 0);
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (int)v;
}

static std::vector<std::string> SplitList(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',' || c == ' ' || c == '|') {
            if (!cur.empty()) { out.push_back(Lower(cur)); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(Lower(cur));
    return out;
}

static std::string ProtectName(uint32_t p) {
    switch (p & 0xFF) {
        case PAGE_NOACCESS:          return "noaccess";
        case PAGE_READONLY:          return "r";
        case PAGE_READWRITE:         return "rw";
        case PAGE_WRITECOPY:         return "rwc";
        case PAGE_EXECUTE:           return "x";
        case PAGE_EXECUTE_READ:      return "rx";
        case PAGE_EXECUTE_READWRITE: return "rwx";
        case PAGE_EXECUTE_WRITECOPY: return "rwxc";
        default:                     return "?";
    }
}

static bool IsReadableProtect(uint32_t p) {
    switch (p & 0xFF) {
        case PAGE_READONLY:
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:
        case PAGE_EXECUTE_READ:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY:
            return true;
        default:
            return false;
    }
}

static std::string BaseName(const std::string& path) {
    size_t p = path.find_last_of("\\/");
    return p == std::string::npos ? path : path.substr(p + 1);
}

std::vector<ModuleEntry> EnumModules() {
    std::vector<ModuleEntry> out;
    auto mods = PatternScan::GetLoadedModules();
    PatternScan::ModuleInfo main = PatternScan::GetMainModule();

    for (const auto& m : mods) {
        ModuleEntry e;
        e.name = BaseName(m.name);
        e.base = (uintptr_t)m.base;
        e.size = m.size;
        e.isMain = (m.base == main.base);

        char path[MAX_PATH] = {};
        if (GetModuleFileNameA((HMODULE)m.base, path, MAX_PATH)) e.path = BaseName(path);
        out.push_back(e);
    }

    std::sort(out.begin(), out.end(),
              [](const ModuleEntry& a, const ModuleEntry& b) { return a.base < b.base; });
    return out;
}

std::vector<MemRegion> EnumRegions(bool includeImages, bool includeMapped,
                                   bool includePrivate, bool readableOnly) {
    std::vector<MemRegion> out;
    auto modules = EnumModules();

    uintptr_t addr = 0;
    MEMORY_BASIC_INFORMATION mbi{};
    while (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        const uintptr_t base = (uintptr_t)mbi.BaseAddress;

        bool keep = false;
        if (mbi.State == MEM_COMMIT) {
            if (mbi.Type == MEM_IMAGE && includeImages)   keep = true;
            if (mbi.Type == MEM_MAPPED && includeMapped)  keep = true;
            if (mbi.Type == MEM_PRIVATE && includePrivate) keep = true;
        }
        if (keep && readableOnly && !IsReadableProtect(mbi.Protect)) keep = false;

        if (keep) {
            MemRegion r;
            r.base = base;
            r.size = mbi.RegionSize;
            r.protect = mbi.Protect;
            r.state = mbi.State;
            r.type = mbi.Type;

            for (const auto& m : modules) {
                if (base >= m.base && base < m.base + m.size) { r.module = BaseName(m.name); break; }
            }
            out.push_back(r);
        }

        uintptr_t next = base + mbi.RegionSize;
        if (next <= addr) break;
        addr = next;
    }
    return out;
}

static std::vector<MemRegion> RegionsForScope(const std::string& scope, uintptr_t base, size_t size) {
    std::vector<MemRegion> regions;

    if (base != 0 || size != 0) {

        MemRegion r;
        r.base = base;
        r.size = size ? size : 0x1000;
        regions.push_back(r);
        return regions;
    }

    const std::string s = Lower(scope);
    if (s == "image" || s == "main") {
        PatternScan::ModuleInfo main = PatternScan::GetMainModule();
        MemRegion r;
        r.base = (uintptr_t)main.base;
        r.size = main.size;
        r.module = BaseName(main.name);
        regions.push_back(r);
        return regions;
    }

    if (s == "module" || s == "modules" || s == "image+module") {
        return EnumRegions(true, true, false, true);
    }

    if (s == "heap") return EnumRegions(false, false, true, true);
    if (s == "exec") {
        auto all = EnumRegions(true, true, true, true);
        std::vector<MemRegion> x;
        for (auto& r : all) {
            if (IsReadableProtect(r.protect) && ((r.protect & 0xFF) == PAGE_EXECUTE ||
                (r.protect & 0xFF) == PAGE_EXECUTE_READ ||
                (r.protect & 0xFF) == PAGE_EXECUTE_READWRITE ||
                (r.protect & 0xFF) == PAGE_EXECUTE_WRITECOPY)) x.push_back(r);
        }
        return x;
    }

    return EnumRegions(true, true, true, true);
}

struct PatternByte {
    uint8_t value = 0;
    uint8_t mask = 0;
};

static bool ParsePattern(const std::string& text, std::vector<PatternByte>& out) {
    std::string cur;
    auto flush = [&]() -> bool {
        if (cur.empty()) return true;
        if (cur == "?" || cur == "??") {
            out.push_back({0, 0});
        } else if (cur.size() > 2 && cur[0] == '0' && (cur[1] == 'x' || cur[1] == 'X')) {
            unsigned v = 0;
            if (sscanf_s(cur.c_str() + 2, "%x", &v) != 1) return false;
            out.push_back({(uint8_t)v, 0xFF});
        } else {
            unsigned v = 0;
            if (sscanf_s(cur.c_str(), "%x", &v) != 1) return false;
            out.push_back({(uint8_t)v, 0xFF});
        }
        cur.clear();
        return true;
    };

    for (char c : text) {
        if (c == ' ' || c == '\t' || c == ',') {
            if (!flush()) return false;
        } else {
            cur += c;
        }
    }
    if (!flush()) return false;
    return !out.empty();
}

static std::vector<uint32_t> BuildFirstByteIndex(const std::vector<uint8_t>& buf,
                                                 uint8_t lower, uint8_t upper,
                                                 bool wildcard) {
    std::vector<uint32_t> idx;
    if (wildcard) return idx;
    idx.reserve(buf.size() / 64);
    for (size_t i = 0; i < buf.size(); ++i) {
        const uint8_t b = buf[i];
        if (b == lower || b == upper) idx.push_back((uint32_t)i);
    }
    return idx;
}

static bool MatchAt(const uint8_t* p, const std::vector<PatternByte>& pat) {
    for (size_t i = 0; i < pat.size(); ++i) {
        if ((p[i] & pat[i].mask) != (pat[i].value & pat[i].mask)) return false;
    }
    return true;
}

std::string SearchPattern(uintptr_t base, size_t size, const std::string& pattern,
                          const std::string& scope, int limit) {
    std::vector<PatternByte> pat;
    if (!ParsePattern(pattern, pat)) {
        return "{\"ok\":false,\"error\":\"invalid pattern; use hex bytes separated by spaces, ?? for a wildcard\"}";
    }

    if (limit <= 0) limit = 64;
    if (limit > 10000) limit = 10000;

    std::vector<MemRegion> regions = RegionsForScope(scope, base, size);

    std::ostringstream oss;
    oss << "{\"ok\":true,\"pattern\":\"" << EscapeJson(pattern) << "\",\"patternBytes\":" << pat.size();

    std::vector<std::pair<uintptr_t, std::string>> hits;
    uint64_t scanned = 0;
    bool truncated = false;

    std::vector<uint8_t> buf;
    for (const auto& r : regions) {
        if (hits.size() >= (size_t)limit) { truncated = true; break; }
        if (r.size == 0 || r.size > (size_t)1 << 34) continue;

        buf.resize(r.size);
        if (SafeMem_ReadSpanExact((const void*)r.base, buf.data(), r.size) != r.size) continue;
        scanned += r.size;

        if (r.size < pat.size()) continue;

        const bool leadingWildcard = (pat[0].mask == 0);
        const std::vector<uint32_t> index =
            BuildFirstByteIndex(buf, pat[0].value, pat[0].value, leadingWildcard);

        if (leadingWildcard) {
            for (size_t i = 0; i + pat.size() <= r.size; ++i) {
                if (!MatchAt(buf.data() + i, pat)) continue;
                hits.push_back({r.base + i, r.module});
                if (hits.size() >= (size_t)limit) { truncated = true; break; }
            }
        } else {
            for (uint32_t i : index) {
                if ((size_t)i + pat.size() > r.size) break;
                if (!MatchAt(buf.data() + i, pat)) continue;
                hits.push_back({r.base + i, r.module});
                if (hits.size() >= (size_t)limit) { truncated = true; break; }
            }
        }
    }

    oss << ",\"scanned\":" << scanned << ",\"count\":" << hits.size()
        << ",\"truncated\":" << (truncated ? "true" : "false") << ",\"hits\":[";
    for (size_t i = 0; i < hits.size(); ++i) {
        if (i) oss << ",";
        oss << "{\"address\":\"" << PtrStr(hits[i].first) << "\"";
        if (!hits[i].second.empty())
            oss << ",\"module\":\"" << EscapeJson(hits[i].second) << "\"";
        oss << "}";
    }
    oss << "]}";
    return oss.str();
}

std::string SearchString(uintptr_t base, size_t size, const std::string& needle,
                         const std::string& scope, int limit, bool utf16) {
    if (needle.empty()) {
        return "{\"ok\":false,\"error\":\"needle is empty\"}";
    }
    if (limit <= 0) limit = 64;
    if (limit > 10000) limit = 10000;

    std::vector<MemRegion> regions = RegionsForScope(scope, base, size);

    std::vector<uint8_t> ascii(needle.begin(), needle.end());
    std::vector<uint8_t> wide;
    if (utf16) {
        for (char c : needle) { wide.push_back((uint8_t)c); wide.push_back(0); }
    }

    std::vector<std::pair<uintptr_t, std::string>> hits;
    uint64_t scanned = 0;
    bool truncated = false;
    std::vector<uint8_t> buf;

    for (const auto& r : regions) {
        if (hits.size() >= (size_t)limit) { truncated = true; break; }
        if (r.size == 0 || r.size > (size_t)1 << 34) continue;

        buf.resize(r.size);
        if (SafeMem_ReadSpanExact((const void*)r.base, buf.data(), r.size) != r.size) continue;
        scanned += r.size;

        const uint8_t c0 = ascii[0];
        const uint8_t c0Upper = (uint8_t)std::toupper(c0);
        const uint8_t c0Lower = (uint8_t)std::tolower(c0);

        {
            const std::vector<uint32_t> index =
                BuildFirstByteIndex(buf, c0Lower, c0Upper, false);
            for (uint32_t i : index) {
                if ((size_t)i + ascii.size() > r.size) break;
                if (memcmp(buf.data() + i, ascii.data(), ascii.size()) != 0) continue;
                hits.push_back({r.base + i, r.module});
                if (hits.size() >= (size_t)limit) { truncated = true; break; }
            }
        }

        if (utf16 && !truncated) {
            const std::vector<uint32_t> index =
                BuildFirstByteIndex(buf, c0Lower, c0Upper, false);
            for (uint32_t i : index) {
                if (i & 1) continue;
                if ((size_t)i + wide.size() > r.size) break;
                if (memcmp(buf.data() + i, wide.data(), wide.size()) != 0) continue;
                hits.push_back({r.base + i, r.module});
                if (hits.size() >= (size_t)limit) { truncated = true; break; }
            }
        }
    }

    std::ostringstream oss;
    oss << "{\"ok\":true,\"needle\":\"" << EscapeJson(needle) << "\",\"utf16\":"
        << (utf16 ? "true" : "false") << ",\"scanned\":" << scanned
        << ",\"count\":" << hits.size()
        << ",\"truncated\":" << (truncated ? "true" : "false") << ",\"hits\":[";
    for (size_t i = 0; i < hits.size(); ++i) {
        if (i) oss << ",";
        oss << "{\"address\":\"" << PtrStr(hits[i].first) << "\"";
        if (!hits[i].second.empty())
            oss << ",\"module\":\"" << EscapeJson(hits[i].second) << "\"";
        oss << "}";
    }
    oss << "]}";
    return oss.str();
}

std::string SearchPointers(uintptr_t base, size_t size, uintptr_t target,
                           const std::string& scope, int limit) {
    if (target == 0) {
        return "{\"ok\":false,\"error\":\"target is required\"}";
    }
    if (limit <= 0) limit = 64;
    if (limit > 10000) limit = 10000;

    std::vector<MemRegion> regions = RegionsForScope(scope, base, size);

    std::vector<std::pair<uintptr_t, std::string>> hits;
    uint64_t scanned = 0;
    bool truncated = false;
    std::vector<uint8_t> buf;

    for (const auto& r : regions) {
        if (hits.size() >= (size_t)limit) { truncated = true; break; }
        if (r.size == 0 || r.size > (size_t)1 << 34) continue;

        buf.resize(r.size);
        if (SafeMem_ReadSpanExact((const void*)r.base, buf.data(), r.size) != r.size) continue;
        scanned += r.size;

        for (size_t i = 0; i + 8 <= r.size; i += 8) {
            uintptr_t v = 0;
            memcpy(&v, buf.data() + i, 8);
            if (v != target) continue;
            hits.push_back({r.base + i, r.module});
            if (hits.size() >= (size_t)limit) { truncated = true; break; }
        }
    }

    std::ostringstream oss;
    oss << "{\"ok\":true,\"target\":\"" << PtrStr(target) << "\",\"scanned\":" << scanned
        << ",\"count\":" << hits.size()
        << ",\"truncated\":" << (truncated ? "true" : "false") << ",\"hits\":[";
    for (size_t i = 0; i < hits.size(); ++i) {
        if (i) oss << ",";
        oss << "{\"address\":\"" << PtrStr(hits[i].first) << "\"";
        if (!hits[i].second.empty())
            oss << ",\"module\":\"" << EscapeJson(hits[i].second) << "\"";
        oss << "}";
    }
    oss << "]}";
    return oss.str();
}

static void AppendProps(std::ostringstream& oss, void* target, int maxProps) {
    PropertyInfoEx props[512] = {};
    int n = GetClassProperties(target, props, 512);
    if (n > maxProps) n = maxProps;

    oss << ",\"propertyCount\":" << n << ",\"properties\":[";
    for (int i = 0; i < n; ++i) {
        if (i) oss << ",";
        oss << "{\"name\":\"" << EscapeJson(props[i].Name) << "\""
            << ",\"type\":\"" << EscapeJson(props[i].TypeName) << "\""
            << ",\"offset\":" << props[i].Offset
            << ",\"offsetHex\":\"0x" << std::hex << props[i].Offset << std::dec << "\"";
        if (props[i].ClassName[0])
            oss << ",\"declaredIn\":\"" << EscapeJson(props[i].ClassName) << "\"";
        if (props[i].bIsArray)  oss << ",\"array\":true";
        if (props[i].bIsMap)    oss << ",\"map\":true";
        if (props[i].bIsSet)    oss << ",\"set\":true";
        if (props[i].bIsStruct) oss << ",\"struct\":true";
        if (props[i].bIsEnum)   oss << ",\"enum\":true";
        if (props[i].bIsObject) oss << ",\"object\":true";
        if (props[i].bIsBool)   oss << ",\"bool\":true";
        if (props[i].bIsString) oss << ",\"string\":true";
        oss << "}";
    }
    oss << "]";
}

static void AppendFuncs(std::ostringstream& oss, void* ustruct, int maxFuncs) {
    if (g_State.UStruct_ChildrenOff < 0 || g_State.UField_NextOff < 0) {
        oss << ",\"functionCount\":0,\"functions\":[]";
        return;
    }

    oss << ",\"functions\":[";
    void* child = EngineDiscovery::GetChildrenHead(g_State, (UStruct*)ustruct);
    int count = 0;
    bool first = true;

    while (child && count < maxFuncs) {
        std::string name = EngineDiscovery::GetObjectNameStr(g_State, (UObject*)child);
        std::string typeName = EngineDiscovery::GetFFieldTypeName(g_State, child);

        std::string cls = typeName;
        if (cls.empty()) {
            UClass* c = EngineDiscovery::GetObjectClass(g_State, (UObject*)child);
            if (c) cls = EngineDiscovery::GetObjectNameStr(g_State, (UObject*)c);
        }

        uint32_t flags = 0;
        if (g_State.UFunction_FlagsOff >= 0) {
            SafeMem_Read((uint8_t*)child + g_State.UFunction_FlagsOff, &flags, 4);
        }

        if (cls.find("Function") != std::string::npos || flags != 0) {
            if (!first) oss << ",";
            first = false;
            oss << "{\"name\":\"" << EscapeJson(name) << "\""
                << ",\"ptr\":\"" << PtrStr((uintptr_t)child) << "\""
                << ",\"flags\":" << flags
                << ",\"flagsHex\":\"0x" << std::hex << flags << std::dec << "\"";
            if (flags & 0x400)      oss << ",\"native\":true";
            if (flags & 0x1)        oss << ",\"final\":true";
            if (flags & 0x200000)   oss << ",\"public\":true";
            if (flags & 0x04000000) oss << ",\"bpCallable\":true";
            if (flags & 0x100)      oss << ",\"blueprintEvent\":true";
            if (flags & 0x4000)     oss << ",\"net\":true";
            oss << "}";
        }

        child = EngineDiscovery::GetFieldNext(g_State, child);
        ++count;
    }

    oss << "],\"functionScanned\":" << count;
}

std::string ClassInfo(const std::string& target) {
    if (!g_State.bInitialized) {
        return "{\"ok\":false,\"error\":\"not initialized\"}";
    }

    std::string t = Trim(target);
    if (t.empty()) {
        return "{\"ok\":false,\"error\":\"usage: CLASSINFO <ptr|ClassName|/Path.Name>\"}";
    }

    void* obj = nullptr;

    bool looksLikePtr = (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X'));
    if (!looksLikePtr && !t.empty() && isdigit((unsigned char)t[0])) looksLikePtr = true;

    if (looksLikePtr) {
        uint64_t v = 0;
        if (ParsePtr(t, v)) obj = (void*)(uintptr_t)v;
    } else {
        obj = EngineDiscovery::FindObjectByNameFast(g_State, t);
        if (!obj) obj = (void*)EngineDiscovery::ResolveObjectPath(g_State, t);
        if (!obj) {
            UClass* c = EngineDiscovery::FindClassCached(g_State, t);
            if (c) obj = c;
        }
    }

    if (!obj) {
        return "{\"ok\":false,\"error\":\"object not found: " + EscapeJson(t) + "\"}";
    }

    std::string name = EngineDiscovery::GetObjectNameStr(g_State, (UObject*)obj);
    std::string full = EngineDiscovery::BuildFullName(g_State, (UObject*)obj);

    UClass* cls = EngineDiscovery::GetObjectClass(g_State, (UObject*)obj);
    std::string clsName = cls ? EngineDiscovery::GetObjectNameStr(g_State, (UObject*)cls) : "";

    bool isClass = (cls && cls == g_State.UClassClass);
    bool isStruct = (cls && g_State.UStructClass &&
                     EngineDiscovery::IsStructA(g_State, (UStruct*)cls, (UStruct*)g_State.UStructClass));
    bool isFunc = (cls && g_State.UFunctionClass &&
                   EngineDiscovery::IsStructA(g_State, (UStruct*)cls, (UStruct*)g_State.UFunctionClass));

    std::ostringstream oss;
    oss << "{\"ok\":true"
        << ",\"name\":\"" << EscapeJson(name) << "\""
        << ",\"path\":\"" << EscapeJson(full) << "\""
        << ",\"address\":\"" << PtrStr((uintptr_t)obj) << "\""
        << ",\"class\":\"" << EscapeJson(clsName) << "\""
        << ",\"isClass\":" << (isClass ? "true" : "false")
        << ",\"isStruct\":" << (isStruct ? "true" : "false")
        << ",\"isFunction\":" << (isFunc ? "true" : "false");

    uint32_t objFlags = 0;
    if (g_State.UObject_FlagsOff >= 0) {
        SafeMem_Read((uint8_t*)obj + g_State.UObject_FlagsOff, &objFlags, 4);
        oss << ",\"objectFlags\":" << objFlags
            << ",\"objectFlagsHex\":\"0x" << std::hex << objFlags << std::dec << "\"";
    }

    std::vector<std::string> chain;
    {
        UStruct* cur = isClass ? (UStruct*)obj : (UStruct*)cls;
        for (int depth = 0; depth < 64 && cur; ++depth) {
            chain.push_back(EngineDiscovery::GetObjectNameStr(g_State, (UObject*)cur));
            cur = EngineDiscovery::GetSuperStruct(g_State, cur);
        }
    }
    oss << ",\"hierarchy\":[";
    for (size_t i = 0; i < chain.size(); ++i) {
        if (i) oss << ",";
        oss << "\"" << EscapeJson(chain[i]) << "\"";
    }
    oss << "]";

    if (g_State.UStruct_ChildPropsOff >= 0) {
        int32_t propsSize = 0;
        if (SafeMem_Read((uint8_t*)obj + g_State.UStruct_ChildPropsOff + 8, &propsSize, 4)) {
            oss << ",\"propertiesSize\":" << propsSize;
        }
        int32_t minAlign = 0;
        if (SafeMem_Read((uint8_t*)obj + g_State.UStruct_ChildPropsOff + 12, &minAlign, 4)) {
            oss << ",\"minAlignment\":" << minAlign;
        }
    }

    if (g_State.UClass_ClassFlagsOff >= 0 && isClass) {
        uint32_t cf = 0;
        if (SafeMem_Read((uint8_t*)obj + g_State.UClass_ClassFlagsOff, &cf, 4)) {
            oss << ",\"classFlags\":" << cf
                << ",\"classFlagsHex\":\"0x" << std::hex << cf << std::dec << "\"";
        }
    }

    if (isClass || isStruct) AppendProps(oss, obj, 1024);
    if (isClass || isStruct || isFunc) AppendFuncs(oss, obj, 2048);

    uintptr_t vtable = 0;
    if (SafeMem_Read(obj, &vtable, sizeof(vtable)) && SafeMem_IsCanonical((void*)vtable)) {
        oss << ",\"vtable\":\"" << PtrStr(vtable) << "\"";
    }

    oss << "}";
    return oss.str();
}

std::string VTable(uintptr_t ptr, int limit) {
    if (ptr == 0) return "{\"ok\":false,\"error\":\"usage: VTABLE <ptr> [limit]\"}";
    if (limit <= 0) limit = 64;
    if (limit > 4096) limit = 4096;

    if (!SafeMem_IsMapped((void*)ptr, sizeof(void*))) {
        return "{\"ok\":false,\"error\":\"pointer is not mapped\"}";
    }

    uintptr_t vtable = 0;
    if (!SafeMem_Read((void*)ptr, &vtable, sizeof(vtable))) {
        return "{\"ok\":false,\"error\":\"failed to read vtable pointer\"}";
    }
    if (!SafeMem_IsCanonical((void*)vtable)) {
        return "{\"ok\":false,\"error\":\"first member is not a canonical pointer\"}";
    }

    auto modules = EnumModules();
    auto ownerOf = [&](uintptr_t a) -> std::string {
        for (const auto& m : modules) {
            if (a >= m.base && a < m.base + m.size) return m.name;
        }
        return "";
    };
    auto ownerBase = [&](uintptr_t a) -> uintptr_t {
        for (const auto& m : modules) {
            if (a >= m.base && a < m.base + m.size) return m.base;
        }
        return 0;
    };

    std::ostringstream oss;
    oss << "{\"ok\":true,\"object\":\"" << PtrStr(ptr) << "\",\"vtable\":\"" << PtrStr(vtable) << "\""
        << ",\"vtableModule\":\"" << EscapeJson(ownerOf(vtable)) << "\",\"entries\":[";

    int emitted = 0;
    for (int i = 0; i < limit; ++i) {
        uintptr_t fn = 0;
        if (!SafeMem_Read((void*)(vtable + (size_t)i * 8), &fn, sizeof(fn))) break;
        if (!SafeMem_IsCanonical((void*)fn)) break;

        std::string mod = ownerOf(fn);
        if (emitted) oss << ",";
        oss << "{\"slot\":" << i << ",\"address\":\"" << PtrStr(fn) << "\""
            << ",\"module\":\"" << EscapeJson(mod) << "\"";
        uintptr_t owner = ownerBase(fn);
        if (owner) oss << ",\"rva\":\"" << PtrStr(fn - owner) << "\"";
        oss << "}";
        ++emitted;
    }

    oss << "],\"count\":" << emitted << "}";
    return oss.str();
}

std::string Modules() {
    auto mods = EnumModules();

    std::ostringstream oss;
    oss << "{\"ok\":true,\"count\":" << mods.size() << ",\"modules\":[";
    for (size_t i = 0; i < mods.size(); ++i) {
        if (i) oss << ",";
        oss << "{\"name\":\"" << EscapeJson(mods[i].name) << "\""
            << ",\"base\":\"" << PtrStr(mods[i].base) << "\""
            << ",\"size\":" << mods[i].size
            << ",\"end\":\"" << PtrStr(mods[i].base + mods[i].size) << "\"";
        if (mods[i].isMain) oss << ",\"main\":true";
        if (!mods[i].path.empty()) oss << ",\"path\":\"" << EscapeJson(mods[i].path) << "\"";
        oss << "}";
    }
    oss << "]}";
    return oss.str();
}

std::string Stats() {
    std::ostringstream oss;
    oss << "{\"ok\":true";

    if (g_State.bInitialized) {
        oss << ",\"objectCount\":" << EngineDiscovery::GetObjectCount(g_State);
    }

    if (g_State.bIndexBuilt && g_State.IndexMutex) {
        std::lock_guard<std::recursive_mutex> lock(*g_State.IndexMutex);
        oss << ",\"indexBuilt\":true"
            << ",\"indexedClasses\":" << g_State.ClassToInstances.size()
            << ",\"indexedNameSlots\":" << g_State.NameToObjects.size()
            << ",\"nameToFirstObjectSlots\":" << g_State.NameToFirstObject.size();

        oss << ",\"sortedNameIndexReady\":"
            << (g_State.SortedNames.empty() ? "false" : "true");
        if (!g_State.SortedNames.empty())
            oss << ",\"indexedNames\":" << g_State.SortedNames.size();
    } else {
        oss << ",\"indexBuilt\":false";
    }

    auto regions = EnumRegions(true, true, true, false);
    uint64_t total = 0, image = 0, mapped = 0, priv = 0;
    int imageN = 0, mappedN = 0, privN = 0;
    for (const auto& r : regions) {
        total += r.size;
        if (r.type == MEM_IMAGE)   { image  += r.size; ++imageN; }
        if (r.type == MEM_MAPPED)  { mapped += r.size; ++mappedN; }
        if (r.type == MEM_PRIVATE) { priv   += r.size; ++privN; }
    }

    oss << ",\"regionCount\":" << regions.size()
        << ",\"regionBytes\":" << total
        << ",\"imageRegions\":" << imageN << ",\"imageBytes\":" << image
        << ",\"mappedRegions\":" << mappedN << ",\"mappedBytes\":" << mapped
        << ",\"privateRegions\":" << privN << ",\"privateBytes\":" << priv
        << ",\"modules\":" << EnumModules().size()
        << "}";
    return oss.str();
}

struct Record {
    uintptr_t ptr = 0;
    std::string name;
    std::string path;
    std::string cls;
    std::string outer;
    int32_t index = -1;
    uint32_t flags = 0;
    int32_t propsSize = -1;
    bool isClass = false;
    bool isActor = false;
};

static std::string FieldList(const std::map<std::string, std::string>& kv) {
    std::string f = GetArg(kv, "fields", "");
    return f;
}

static bool WantsField(const std::vector<std::string>& fields, const char* key) {
    if (fields.empty()) return true;
    for (const auto& f : fields) if (f == key) return true;
    return false;
}

std::string HandleQuery(const std::string& args) {
    std::map<std::string, std::string> kv = ParseArgs(args);
    std::string action = Lower(GetArg(kv, "action", "find"));

    if (!g_State.bInitialized) {
        return "{\"ok\":false,\"error\":\"not initialized\"}";
    }

    if (action == "find" || action == "objects" || action == "search") {
        const std::string nameContains = Lower(GetArg(kv, "name"));
        const std::string nameExact    = Lower(GetArg(kv, "exact"));
        const std::string pathContains = Lower(GetArg(kv, "path"));
        const std::string className    = Lower(GetArg(kv, "class"));
        const std::string outerName    = Lower(GetArg(kv, "outer"));
        const std::string regexTerm    = Lower(GetArg(kv, "match"));
        const int limit = GetIntArg(kv, "limit", 200, 1, 100000);
        const int offset = GetIntArg(kv, "offset", 0, 0, 1 << 30);
        const bool wantActors = Lower(GetArg(kv, "actor")) == "true";
        const bool wantClasses = Lower(GetArg(kv, "classonly")) == "true";

        std::vector<std::string> fields = SplitList(FieldList(kv));

        std::vector<Record> records;
        const int total = EngineDiscovery::GetObjectCount(g_State);

        UClass* filterClass = nullptr;
        bool filterDerived = false;
        if (!className.empty()) {
            std::string want = className;
            if (!want.empty() && want[0] == '-') { filterDerived = true; want = want.substr(1); }
            filterClass = EngineDiscovery::FindClassCached(g_State, want);
            if (!filterClass) filterClass = (UClass*)EngineDiscovery::FindObjectByNameFast(g_State, want);
        }

        for (int i = 0; i < total; ++i) {
            UObject* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
            if (!obj) continue;

            std::string nm = EngineDiscovery::GetObjectNameStr(g_State, obj);
            if (nm.empty()) continue;

            std::string nmLower = Lower(nm);

            if (!nameExact.empty() && nmLower != nameExact) continue;
            if (!nameContains.empty() && nmLower.find(nameContains) == std::string::npos) continue;

            UClass* cls = EngineDiscovery::GetObjectClass(g_State, obj);
            std::string clsName = cls ? EngineDiscovery::GetObjectNameStr(g_State, (UObject*)cls) : "";

            if (filterClass && cls) {
                bool ok = filterDerived
                    ? EngineDiscovery::IsStructA(g_State, (UStruct*)cls, (UStruct*)filterClass)
                    : (cls == filterClass ||
                       Lower(clsName) == className ||
                       EngineDiscovery::IsStructA(g_State, (UStruct*)cls, (UStruct*)filterClass));
                if (!ok) continue;
            } else if (!className.empty() && !filterClass) {
                if (Lower(clsName).find(className) == std::string::npos) continue;
            }

            bool isClass = (cls && cls == g_State.UClassClass);
            if (wantClasses && !isClass) continue;

            bool isActor = (cls && g_State.AActorClass &&
                            EngineDiscovery::IsStructA(g_State, (UStruct*)cls, (UStruct*)g_State.AActorClass));
            if (wantActors && !isActor) continue;

            if (!pathContains.empty()) {
                std::string full = Lower(EngineDiscovery::BuildFullName(g_State, obj));
                if (full.find(pathContains) == std::string::npos) continue;
            }

            if (!outerName.empty()) {
                UObject* outer = EngineDiscovery::GetObjectOuter(g_State, obj);
                std::string on = outer ? Lower(EngineDiscovery::GetObjectNameStr(g_State, outer)) : "";
                if (on.find(outerName) == std::string::npos) continue;
            }

            if (!regexTerm.empty()) {

                if (nmLower.find(regexTerm) == std::string::npos &&
                    Lower(clsName).find(regexTerm) == std::string::npos) continue;
            }

            Record r;
            r.ptr = (uintptr_t)obj;
            r.name = nm;
            r.cls = clsName;
            r.index = i;
            r.isClass = isClass;
            r.isActor = isActor;

            if (WantsField(fields, "path")) {
                r.path = EngineDiscovery::BuildFullName(g_State, obj);
            }
            if (WantsField(fields, "flags") && g_State.UObject_FlagsOff >= 0) {
                SafeMem_Read((uint8_t*)obj + g_State.UObject_FlagsOff, &r.flags, 4);
            }
            if (WantsField(fields, "size") && g_State.UStruct_ChildPropsOff >= 0) {
                SafeMem_Read((uint8_t*)obj + g_State.UStruct_ChildPropsOff + 8, &r.propsSize, 4);
            }
            if (WantsField(fields, "outer")) {
                UObject* outer = EngineDiscovery::GetObjectOuter(g_State, obj);
                r.outer = outer ? EngineDiscovery::GetObjectNameStr(g_State, outer) : "";
            }

            records.push_back(std::move(r));
            if ((int)records.size() >= limit + offset) break;
        }

        const int matched = (int)records.size();
        std::ostringstream oss;
        oss << "{\"ok\":true,\"action\":\"find\",\"scanned\":" << total
            << ",\"matched\":" << matched
            << ",\"offset\":" << offset
            << ",\"limit\":" << limit
            << ",\"records\":[";

        int emitted = 0;
        for (size_t i = (size_t)offset; i < records.size(); ++i) {
            if (emitted) oss << ",";
            const Record& r = records[i];
            oss << "{\"ptr\":\"" << PtrStr(r.ptr) << "\"";
            if (WantsField(fields, "name"))  oss << ",\"name\":\"" << EscapeJson(r.name) << "\"";
            if (WantsField(fields, "class")) oss << ",\"class\":\"" << EscapeJson(r.cls) << "\"";
            if (WantsField(fields, "path") && !r.path.empty())
                oss << ",\"path\":\"" << EscapeJson(r.path) << "\"";
            if (WantsField(fields, "outer") && !r.outer.empty())
                oss << ",\"outer\":\"" << EscapeJson(r.outer) << "\"";
            if (WantsField(fields, "index"))  oss << ",\"index\":" << r.index;
            if (WantsField(fields, "flags"))  oss << ",\"flags\":" << r.flags
                << ",\"flagsHex\":\"0x" << std::hex << r.flags << std::dec << "\"";
            if (WantsField(fields, "size") && r.propsSize >= 0) oss << ",\"size\":" << r.propsSize;
            if (r.isClass) oss << ",\"isClass\":true";
            if (r.isActor) oss << ",\"isActor\":true";
            oss << "}";
            ++emitted;
        }
        oss << "],\"returned\":" << emitted << "}";
        return oss.str();
    }

    if (action == "props" || action == "properties") {
        std::string target = GetArg(kv, "target", GetArg(kv, "name"));
        if (target.empty()) return "{\"ok\":false,\"error\":\"target is required\"}";

        void* obj = nullptr;
        uint64_t v = 0;
        if (ParsePtr(target, v) && v > 0x10000) obj = (void*)(uintptr_t)v;
        if (!obj) obj = EngineDiscovery::FindObjectByNameFast(g_State, target);
        if (!obj) obj = (void*)EngineDiscovery::ResolveObjectPath(g_State, target);
        if (!obj) return "{\"ok\":false,\"error\":\"object not found\"}";

        int limit = GetIntArg(kv, "limit", 512, 1, 4096);
        std::ostringstream oss;
        oss << "{\"ok\":true,\"action\":\"props\"";
        AppendProps(oss, obj, limit);
        oss << "}";
        return oss.str();
    }

    if (action == "funcs" || action == "functions") {
        std::string target = GetArg(kv, "target", GetArg(kv, "name"));
        if (target.empty()) return "{\"ok\":false,\"error\":\"target is required\"}";

        void* obj = nullptr;
        uint64_t v = 0;
        if (ParsePtr(target, v) && v > 0x10000) obj = (void*)(uintptr_t)v;
        if (!obj) obj = EngineDiscovery::FindObjectByNameFast(g_State, target);
        if (!obj) obj = (void*)EngineDiscovery::ResolveObjectPath(g_State, target);
        if (!obj) return "{\"ok\":false,\"error\":\"object not found\"}";

        UClass* cls = EngineDiscovery::GetObjectClass(g_State, (UObject*)obj);
        void* search = (cls && cls == g_State.UClassClass) ? obj : (void*)cls;
        if (!search) return "{\"ok\":false,\"error\":\"no class\"}";

        int limit = GetIntArg(kv, "limit", 2048, 1, 8192);
        std::ostringstream oss;
        oss << "{\"ok\":true,\"action\":\"funcs\"";
        AppendFuncs(oss, search, limit);
        oss << "}";
        return oss.str();
    }

    if (action == "class" || action == "classinfo") {
        return ClassInfo(GetArg(kv, "target", GetArg(kv, "name")));
    }

    if (action == "vtable" || action == "vft") {
        std::string target = GetArg(kv, "target", GetArg(kv, "ptr"));
        uint64_t v = 0;
        if (!ParsePtr(target, v)) return "{\"ok\":false,\"error\":\"target must be a pointer\"}";
        return VTable((uintptr_t)v, GetIntArg(kv, "limit", 64, 1, 4096));
    }

    if (action == "pattern" || action == "scan" || action == "aob") {
        std::string pat = GetArg(kv, "pattern", GetArg(kv, "bytes"));
        if (pat.empty()) return "{\"ok\":false,\"error\":\"pattern is required\"}";
        uint64_t base = 0, size = 0;
        ParsePtr(GetArg(kv, "base", "0"), base);
        ParseU64(GetArg(kv, "size", "0"), size);
        return SearchPattern((uintptr_t)base, (size_t)size, pat,
                             GetArg(kv, "scope", "image"), GetIntArg(kv, "limit", 64, 1, 10000));
    }

    if (action == "string" || action == "str") {
        std::string needle = GetArg(kv, "needle", GetArg(kv, "text"));
        if (needle.empty()) return "{\"ok\":false,\"error\":\"needle is required\"}";
        uint64_t base = 0, size = 0;
        ParsePtr(GetArg(kv, "base", "0"), base);
        ParseU64(GetArg(kv, "size", "0"), size);
        bool utf16 = Lower(GetArg(kv, "utf16", "true")) != "false";
        return SearchString((uintptr_t)base, (size_t)size, needle,
                            GetArg(kv, "scope", "module"), GetIntArg(kv, "limit", 64, 1, 10000), utf16);
    }

    if (action == "pointers" || action == "xref" || action == "refs") {
        uint64_t target = 0;
        if (!ParsePtr(GetArg(kv, "target", GetArg(kv, "ptr")), target) || target == 0) {
            return "{\"ok\":false,\"error\":\"target pointer is required\"}";
        }
        uint64_t base = 0, size = 0;
        ParsePtr(GetArg(kv, "base", "0"), base);
        ParseU64(GetArg(kv, "size", "0"), size);
        return SearchPointers((uintptr_t)base, (size_t)size, (uintptr_t)target,
                              GetArg(kv, "scope", "all"), GetIntArg(kv, "limit", 64, 1, 10000));
    }

    if (action == "modules" || action == "mods") return Modules();
    if (action == "stats" || action == "info") return Stats();

    if (action == "regions") {
        bool images  = Lower(GetArg(kv, "images", "true")) != "false";
        bool mapped  = Lower(GetArg(kv, "mapped", "true")) != "false";
        bool priv    = Lower(GetArg(kv, "private", "true")) != "false";
        bool readable = Lower(GetArg(kv, "readable", "true")) != "false";
        int limit = GetIntArg(kv, "limit", 2000, 1, 200000);

        auto regions = EnumRegions(images, mapped, priv, readable);
        std::ostringstream oss;
        oss << "{\"ok\":true,\"action\":\"regions\",\"count\":" << regions.size() << ",\"regions\":[";
        int n = 0;
        for (const auto& r : regions) {
            if (n >= limit) break;
            if (n) oss << ",";
            oss << "{\"base\":\"" << PtrStr(r.base) << "\""
                << ",\"size\":" << r.size
                << ",\"end\":\"" << PtrStr(r.base + r.size) << "\""
                << ",\"protect\":\"" << ProtectName(r.protect) << "\""
                << ",\"type\":\""
                << (r.type == MEM_IMAGE ? "image" : r.type == MEM_MAPPED ? "mapped" : "private")
                << "\"";
            if (!r.module.empty()) oss << ",\"module\":\"" << EscapeJson(r.module) << "\"";
            oss << "}";
            ++n;
        }
        oss << "],\"returned\":" << n << "}";
        return oss.str();
    }

    return "{\"ok\":false,\"error\":\"unknown action: " + EscapeJson(action) +
           "\",\"actions\":[\"find\",\"props\",\"funcs\",\"class\",\"vtable\",\"pattern\","
           "\"string\",\"pointers\",\"modules\",\"regions\",\"stats\"]}";
}

}
