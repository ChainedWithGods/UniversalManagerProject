#include "AESKeyFinder.h"
#include "../core/EngineDiscovery.h"
#include "../utils/SafeMemory.h"

#include <windows.h>
#include <winver.h>
#include <psapi.h>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <format>
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>
#include <map>
#include <set>

namespace fs = std::filesystem;

extern HMODULE g_Module;

namespace Crypto {

static AESKeyResult      s_Result;
static std::atomic<bool> s_Done{false};
static std::mutex        s_Mutex;

static const double PROB_LOG2_TABLE[33] = {
    0.0,
    0.156250000000, 0.250000000000, 0.320090028751, 0.375000000000,
    0.418465035938, 0.452819531115, 0.479532898687, 0.500000000000,
    0.515234475438, 0.525992928509, 0.532892972986, 0.536449514750,
    0.537061730048, 0.535038318468, 0.530612140411, 0.500000000000,
    0.515234475438, 0.504260270822, 0.490799767228, 0.474640103738,
    0.455581566874, 0.433405786637, 0.407883204987, 0.378774780375,
    0.345828469812, 0.308774780375, 0.267323864789, 0.221168903525,
    0.169984920626, 0.113428905312, 0.051152955312, 0.000000000000
};

double AESKeyFinder::CalculateEntropy(const uint8_t* p, size_t len) {
    if (len != 32) {
        int freq[256] = {};
        for (size_t i = 0; i < len; ++i) freq[p[i]]++;
        double e = 0.0;
        for (int i = 0; i < 256; ++i) {
            if (!freq[i]) continue;
            const double prob = static_cast<double>(freq[i]) / len;
            e -= prob * std::log2(prob);
        }
        return e;
    }
    int freq[256] = {};
    for (int i = 0; i < 32; ++i) freq[p[i]]++;
    double ent = 0.0;
    for (int i = 0; i < 256; ++i)
        if (freq[i]) ent += PROB_LOG2_TABLE[freq[i]];
    return ent;
}

std::string AESKeyFinder::FormatHex(const uint8_t* data, size_t len, bool compact) {
    std::string out = "0x";
    for (size_t i = 0; i < len; ++i) {
        if (!compact && i > 0) out += ' ';
        out += std::format("{:02X}", data[i]);
    }
    return out;
}

std::string AESKeyFinder::FormatBase64(const uint8_t* data, size_t len) {
    static const char b64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    size_t i = 0;
    while (i < len) {
        const size_t rem = len - i;
        const uint32_t a = data[i++];
        const uint32_t b = (rem > 1) ? data[i++] : 0;
        const uint32_t c = (rem > 2) ? data[i++] : 0;
        const uint32_t triple = (a << 16) | (b << 8) | c;

        out.push_back(b64_chars[(triple >> 18) & 0x3F]);
        out.push_back(b64_chars[(triple >> 12) & 0x3F]);
        out.push_back((rem > 1) ? b64_chars[(triple >> 6) & 0x3F] : '=');
        out.push_back((rem > 2) ? b64_chars[triple & 0x3F] : '=');
    }
    return out;
}

std::string AESKeyFinder::FormatCArray(const uint8_t* data, size_t len) {
    std::string out = "{ ";
    for (size_t i = 0; i < len; ++i) {
        if (i > 0) out += ", ";
        out += std::format("0x{:02X}", data[i]);
    }
    out += " }";
    return out;
}

bool AESKeyFinder::LooksLikeKey(const uint8_t* p) {
    const uint8_t first = p[0];
    bool all_same = true;
    for (int i = 1; i < 32; ++i) {
        if (p[i] != first) { all_same = false; break; }
    }
    if (all_same) return false;

    int freq[256] = {};
    int unique_count = 0, printable = 0;
    for (int i = 0; i < 32; ++i) {
        const uint8_t b = p[i];
        if (!freq[b]++) unique_count++;
        if (b >= 0x20 && b <= 0x7E) printable++;
    }

    if (unique_count < 14) return false;
    if (freq[0] > 4)       return false;
    if (printable >= 28)   return false;
    if (freq[0x00] >= 3 && (freq[0x3F] >= 2 || freq[0xBF] >= 2)) return false;

    double ent = 0.0;
    for (int i = 0; i < 256; ++i)
        if (freq[i]) ent += PROB_LOG2_TABLE[freq[i]];
    if (ent < 3.5) return false;

    int max_run = 1, cur_run = 1;
    for (int i = 1; i < 32; ++i) {
        if (p[i] == p[i - 1]) { if (++cur_run > max_run) max_run = cur_run; }
        else cur_run = 1;
    }
    return max_run < 5;
}

static std::string TrimAscii(std::string value) {
    size_t begin = 0, end = value.size();
    while (begin < end && (value[begin] == ' ' || value[begin] == '\t')) ++begin;
    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t')) --end;
    return value.substr(begin, end - begin);
}

static std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                         static_cast<int>(value.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return {};
    std::string out(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        out.data(), bytes, nullptr, nullptr);
    return out;
}

static ExeVersionInfo QueryVersionInfo(const std::string& exe_path) {
    ExeVersionInfo out;
    HMODULE version = LoadLibraryW(L"version.dll");
    if (!version) return out;

    auto get_size = reinterpret_cast<DWORD(WINAPI*)(LPCWSTR, LPDWORD)>(
        GetProcAddress(version, "GetFileVersionInfoSizeW"));
    auto get_info = reinterpret_cast<BOOL(WINAPI*)(LPCWSTR, DWORD, DWORD, LPVOID)>(
        GetProcAddress(version, "GetFileVersionInfoW"));
    auto query = reinterpret_cast<BOOL(WINAPI*)(LPCVOID, LPCWSTR, LPVOID*, PUINT)>(
        GetProcAddress(version, "VerQueryValueW"));

    if (get_size && get_info && query) {
        const std::wstring wide_path = fs::path(exe_path).wstring();
        DWORD ignored = 0;
        const DWORD size = get_size(wide_path.c_str(), &ignored);
        if (size > 0) {
            std::vector<uint8_t> data(size);
            if (get_info(wide_path.c_str(), ignored, size, data.data())) {
                auto query_string = [&](const wchar_t* sub) -> std::string {
                    void* pointer = nullptr;
                    UINT length = 0;
                    if (!query(data.data(), sub, &pointer, &length) || !pointer || length == 0)
                        return {};
                    std::wstring value(static_cast<const wchar_t*>(pointer), length);
                    const size_t nul = value.find(L'\0');
                    if (nul != std::wstring::npos) value.resize(nul);
                    return TrimAscii(WideToUtf8(value));
                };

                VS_FIXEDFILEINFO* fixed = nullptr;
                UINT fixed_length = 0;
                if (query(data.data(), L"\\", reinterpret_cast<void**>(&fixed), &fixed_length) &&
                    fixed && fixed_length >= sizeof(VS_FIXEDFILEINFO)) {
                    out.fixed_file_version = std::format("{}.{}.{}.{}",
                        fixed->dwFileVersionMS >> 16, fixed->dwFileVersionMS & 0xFFFF,
                        fixed->dwFileVersionLS >> 16, fixed->dwFileVersionLS & 0xFFFF);
                    out.fixed_product_version = std::format("{}.{}.{}.{}",
                        fixed->dwProductVersionMS >> 16, fixed->dwProductVersionMS & 0xFFFF,
                        fixed->dwProductVersionLS >> 16, fixed->dwProductVersionLS & 0xFFFF);
                    out.product_major = fixed->dwProductVersionMS >> 16;
                    out.product_minor = fixed->dwProductVersionMS & 0xFFFF;
                }

                struct LANGANDCODEPAGE { WORD language; WORD code_page; };
                LANGANDCODEPAGE* translations = nullptr;
                UINT translation_bytes = 0;
                WORD language = 0x0409, code_page = 0x04B0;
                if (query(data.data(), L"\\VarFileInfo\\Translation",
                          reinterpret_cast<void**>(&translations), &translation_bytes) &&
                    translations && translation_bytes >= sizeof(LANGANDCODEPAGE)) {
                    language = translations[0].language;
                    code_page = translations[0].code_page;
                }

                auto get_string = [&](const wchar_t* name) {
                    wchar_t sub[256];
                    swprintf_s(sub, 256, L"\\StringFileInfo\\%04x%04x\\%s",
                               language, code_page, name);
                    return query_string(sub);
                };

                out.product_name = get_string(L"ProductName");
                out.file_description = get_string(L"FileDescription");
                out.company_name = get_string(L"CompanyName");
                out.file_version = get_string(L"FileVersion");
                out.product_version = get_string(L"ProductVersion");
                out.original_filename = get_string(L"OriginalFilename");
                out.internal_name = get_string(L"InternalName");
                out.valid = !out.product_name.empty() || !out.file_description.empty() ||
                            !out.fixed_product_version.empty();
            }
        }
    }
    FreeLibrary(version);
    return out;
}

static std::string RvaToSectionName(uintptr_t module_base, uint64_t rva) {
    if (!module_base) return {};
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module_base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return {};
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(module_base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return {};
    const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        const uint64_t start = section->VirtualAddress;
        const uint64_t size = std::max<uint64_t>(section->Misc.VirtualSize,
                                                 section->SizeOfRawData);
        if (rva >= start && rva < start + size) {
            char name[9] = {};
            memcpy(name, section->Name, 8);
            return name;
        }
    }
    return {};
}

class JsonOut {
public:
    std::string text;

    void beginObject(const char* key = nullptr) { start(key, '{'); }
    void beginArray(const char* key = nullptr) { start(key, '['); }
    void endObject() { finish('}'); }
    void endArray() { finish(']'); }

    void field(const char* key, const std::string& value) {
        item(key);
        text += quote(value);
    }
    void field(const char* key, const char* value) {
        item(key);
        text += quote(value);
    }
    void fieldInt(const char* key, int64_t value) {
        item(key);
        text += std::to_string(value);
    }
    void fieldUInt(const char* key, uint64_t value) {
        item(key);
        text += std::to_string(value);
    }
    void fieldBool(const char* key, bool value) {
        item(key);
        text += value ? "true" : "false";
    }
    void fieldRaw(const char* key, const std::string& raw) {
        item(key);
        text += raw;
    }

private:
    std::vector<bool> first_;

    static std::string quote(const std::string& value) {
        static const char hex[] = "0123456789ABCDEF";
        std::string out;
        out.reserve(value.size() + 8);
        out.push_back('"');
        for (unsigned char c : value) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out.push_back(hex[c >> 4]);
                    out.push_back(hex[c & 0x0F]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
            }
        }
        out.push_back('"');
        return out;
    }

    void item(const char* key) {
        if (!first_.empty()) {
            if (!first_.back()) text += ',';
            first_.back() = false;
            text += '\n';
            text.append(first_.size() * 2, ' ');
        }
        if (key) {
            text += quote(key);
            text += ": ";
        }
    }

    void start(const char* key, char delimiter) {
        item(key);
        text += delimiter;
        first_.push_back(true);
    }

    void finish(char delimiter) {
        const bool empty = first_.back();
        first_.pop_back();
        if (!empty) {
            text += '\n';
            text.append(first_.size() * 2, ' ');
        }
        text += delimiter;
    }
};

static std::string GuidString(const uint8_t* guid) {
    static const char digits[] = "0123456789ABCDEF";
    std::string hex;
    hex.reserve(32);
    for (int i = 0; i < 16; ++i) {
        hex.push_back(digits[guid[i] >> 4]);
        hex.push_back(digits[guid[i] & 0x0F]);
    }
    return hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-" + hex.substr(12, 4) +
           "-" + hex.substr(16, 4) + "-" + hex.substr(20, 12);
}

static std::string FirstKeyGuid(const std::vector<ArchiveReport>& archives) {
    for (const auto& archive : archives) {
        if (archive.kind == "pak" && archive.footer.valid)
            return GuidString(archive.footer.encryption_key_guid);
    }
    for (const auto& archive : archives) {
        if (archive.kind == "iostore_toc" && archive.toc.valid)
            return GuidString(archive.toc.encryption_key_guid);
    }
    return "00000000-0000-0000-0000-000000000000";
}

static std::vector<std::string> ContainerFlagNames(uint8_t flags) {
    std::vector<std::string> names;
    if (flags & 1) names.emplace_back("Compressed");
    if (flags & 2) names.emplace_back("Encrypted");
    if (flags & 4) names.emplace_back("Signed");
    if (flags & 8) names.emplace_back("Indexed");
    if (flags & 16) names.emplace_back("OnDemand");
    return names;
}

static void EmitStringArray(JsonOut& json, const char* key,
                            const std::vector<std::string>& values) {
    json.beginArray(key);
    for (const auto& value : values) json.field(nullptr, value);
    json.endArray();
}

static void EmitExtensionCounts(JsonOut& json, const char* key,
                                const std::map<std::string, int>& extensions) {
    std::vector<std::pair<std::string, int>> sorted(extensions.begin(), extensions.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });
    if (sorted.size() > 32) sorted.resize(32);
    json.beginObject(key);
    for (const auto& [extension, count] : sorted)
        json.fieldInt(extension.c_str(), count);
    json.endObject();
}

static void EmitIndexReport(JsonOut& json, const char* key, const IndexReport& index) {
    json.beginObject(key);
    json.fieldBool("present", index.present);
    if (index.present) {
        json.fieldUInt("offset", static_cast<uint64_t>(index.offset));
        json.fieldUInt("size", static_cast<uint64_t>(index.size));
        if (index.serialized_size)
            json.fieldUInt("serialized_size", static_cast<uint64_t>(index.serialized_size));
        if (index.padding_bytes)
            json.fieldUInt("aes_padding_bytes", static_cast<uint64_t>(index.padding_bytes));
        json.fieldBool("encrypted", index.encrypted);
        if (index.has_hash) {
            json.field("stored_sha1", index.stored_sha1);
            json.field("computed_sha1", index.computed_sha1);
            json.fieldBool("hash_verified", index.hash_verified);
        }
        if (!index.error.empty()) json.field("error", index.error);
    }
    json.endObject();
}

static void EmitSampleEntry(JsonOut& json, const PakSampleEntry& entry) {
    json.beginObject();
    json.field("path", entry.path);
    if (!entry.error.empty()) {
        json.field("error", entry.error);
    } else {
        json.fieldUInt("encoded_offset", static_cast<uint64_t>(entry.encoded_offset));
        json.fieldUInt("offset", static_cast<uint64_t>(entry.offset));
        json.fieldUInt("size", static_cast<uint64_t>(entry.size));
        json.fieldUInt("uncompressed_size", static_cast<uint64_t>(entry.uncompressed_size));
        json.fieldInt("compression_method_index", entry.compression_method_index);
        json.field("compression_method", entry.compression_method);
        json.fieldBool("encrypted", entry.encrypted);
        json.fieldUInt("compression_block_count", entry.compression_block_count);
        json.fieldUInt("compression_block_size", entry.compression_block_size);
    }
    json.endObject();
}

static void EmitDirectoryReport(JsonOut& json, const char* key,
                                const DirectoryReport& directory, bool iostore) {
    json.beginObject(key);
    json.fieldBool("present", directory.present);
    if (!directory.present) {
        json.endObject();
        return;
    }
    json.fieldBool("encrypted", directory.encrypted);
    json.fieldBool("decrypted", directory.decrypted);
    json.fieldUInt("offset", static_cast<uint64_t>(directory.offset));
    json.fieldUInt("size", static_cast<uint64_t>(directory.size));
    if (!directory.stored_sha1.empty()) json.field("stored_sha1", directory.stored_sha1);
    if (!directory.computed_sha1.empty()) json.field("computed_sha1", directory.computed_sha1);
    if (!directory.stored_sha1.empty()) json.fieldBool("hash_verified", directory.hash_verified);
    if (directory.serialized_size)
        json.fieldUInt("serialized_size", static_cast<uint64_t>(directory.serialized_size));
    if (directory.padding_bytes)
        json.fieldUInt("aes_padding_bytes", static_cast<uint64_t>(directory.padding_bytes));
    json.field("mount_point", directory.mount_point);
    if (iostore) {
        json.fieldInt("directory_entries", directory.directory_count);
        json.fieldInt("file_entries", directory.file_count);
        json.fieldInt("string_table_entries", directory.string_count);
        json.fieldInt("walked_file_count", directory.walked_file_count);
    } else {
        json.fieldInt("directory_count", directory.directory_count);
        json.fieldInt("file_count", directory.file_count);
    }
    EmitExtensionCounts(json, "extension_counts", directory.extension_counts);
    EmitStringArray(json, "sample_paths", directory.sample_paths);
    if (!directory.sample_entries.empty()) {
        json.beginArray("sample_entries");
        for (const auto& entry : directory.sample_entries) EmitSampleEntry(json, entry);
        json.endArray();
    }
    if (!directory.sample_files.empty()) {
        json.beginArray("sample_files");
        for (const auto& [path, chunk] : directory.sample_files) {
            json.beginObject();
            json.field("path", path);
            json.fieldUInt("chunk_index", chunk);
            json.endObject();
        }
        json.endArray();
    }
    if (!directory.error.empty()) json.field("error", directory.error);
    json.endObject();
}

static void EmitArchive(JsonOut& json, const ArchiveReport& archive) {
    json.beginObject();
    json.field("file_name", archive.file_name);
    json.field("kind", archive.kind);
    json.fieldUInt("file_size", archive.file_size);
    json.fieldBool("encrypted", archive.encrypted);
    if (!archive.error.empty()) json.field("error", archive.error);

    if (archive.kind == "pak") {
        json.beginObject("footer");
        json.fieldUInt("offset", archive.footer.offset);
        json.fieldUInt("size", archive.footer.size);
        json.fieldBool("ends_at_eof", archive.footer.ends_at_eof);
        json.field("magic", "0x5A6F12E1");
        json.fieldInt("version", archive.footer.version);
        json.field("version_name", archive.footer.version_name);
        json.fieldBool("encrypted_index", archive.footer.encrypted_index);
        json.field("encryption_key_guid", GuidString(archive.footer.encryption_key_guid));
        EmitStringArray(json, "compression_methods", archive.footer.compression_methods);
        json.endObject();

        json.beginObject("primary_index");
        json.fieldUInt("offset", archive.primary_index.offset);
        json.fieldUInt("size", archive.primary_index.size);
        json.fieldBool("encrypted", archive.primary_index.encrypted);
        if (archive.primary_index.serialized_size)
            json.fieldUInt("serialized_size", archive.primary_index.serialized_size);
        if (archive.primary_index.padding_bytes)
            json.fieldUInt("aes_padding_bytes", archive.primary_index.padding_bytes);
        json.field("mount_point", archive.mount_point);
        json.fieldInt("entry_count", archive.entry_count);
        json.field("path_hash_seed", std::format("0x{:016X}", archive.path_hash_seed));
        json.fieldBool("hash_verified", archive.primary_index.hash_verified);
        json.field("stored_sha1", archive.primary_index.stored_sha1);
        json.field("computed_sha1", archive.primary_index.computed_sha1);
        json.fieldBool("has_path_hash_index", archive.has_path_hash_index);
        json.fieldBool("has_full_directory_index", archive.has_full_directory_index);
        json.fieldUInt("encoded_entries_size", static_cast<uint64_t>(archive.encoded_entries_size));
        json.fieldInt("unencoded_entries", archive.unencoded_entries);
        if (!archive.primary_index.error.empty())
            json.field("error", archive.primary_index.error);
        if (archive.has_path_hash_index)
            EmitIndexReport(json, "path_hash_index", archive.path_hash_index);
        if (archive.has_full_directory_index)
            EmitIndexReport(json, "full_directory_index", archive.full_directory_index);
        json.endObject();

        EmitDirectoryReport(json, "directory", archive.directory, false);
    } else if (archive.kind == "iostore_toc") {
        json.beginObject("toc_header");
        json.fieldInt("version", archive.toc.version);
        json.field("version_name", archive.toc.version_name);
        json.fieldUInt("header_size", archive.toc.header_size);
        json.field("container_id", std::format("{:016X}", archive.toc.container_id));
        json.fieldBool("container_id_valid", archive.toc.container_id != ~0ull);
        if (archive.toc.container_id == ~0ull) {
            json.field("container_id_note",
                       "FIoContainerId::InvalidId (uint64(-1)); the global container has no assigned container ID.");
        }
        json.fieldUInt("container_flags", archive.toc.flags);
        EmitStringArray(json, "container_flag_names", ContainerFlagNames(archive.toc.flags));
        json.field("encryption_key_guid", GuidString(archive.toc.encryption_key_guid));
        json.fieldUInt("entry_count", archive.toc.entry_count);
        json.fieldUInt("compressed_block_count", archive.toc.block_count);
        json.fieldUInt("compressed_block_entry_size", archive.toc.block_entry_size);
        json.fieldUInt("compression_block_size", archive.toc.block_size);
        EmitStringArray(json, "compression_methods", archive.toc.methods);
        json.fieldUInt("partition_count", archive.toc.partition_count);
        json.fieldUInt("partition_size", archive.toc.partition_size);
        json.fieldUInt("perfect_hash_seed_count", archive.toc.seed_count);
        json.fieldUInt("chunks_without_perfect_hash", archive.toc.without_hash_count);
        json.fieldUInt("directory_index_size", archive.toc.dir_size);
        json.fieldUInt("directory_index_offset", archive.toc.dir_offset);
        json.fieldUInt("signature_bytes", archive.toc.signature_bytes);
        json.fieldUInt("entry_meta_size_bytes", 33);
        json.fieldUInt("entry_meta_total_bytes",
                       static_cast<uint64_t>(archive.toc.entry_count) * 33);
        json.fieldUInt("metadata_tail_bytes", archive.toc.meta_bytes);
        json.fieldBool("metadata_tail_consistent", archive.toc.meta_consistent);
        json.beginObject("chunk_type_counts");
        for (const auto& [name, count] : archive.toc.chunk_type_counts)
            json.fieldInt(name.c_str(), count);
        json.endObject();
        json.endObject();

        if (archive.utoc_directory.present)
            EmitDirectoryReport(json, "directory_index", archive.utoc_directory, true);
        else
            json.fieldRaw("directory_index", "null");
        if (!archive.paired_path.empty()) {
            std::string paired = fs::path(archive.paired_path).filename().string();
            json.field("paired_container", paired);
        }
    } else {
        if (!archive.paired_path.empty()) {
            std::string paired = fs::path(archive.paired_path).filename().string();
            json.field("paired_toc", paired);
        }
        json.fieldBool("chunk_data_encrypted", archive.chunk_data_encrypted);
    }
    json.endObject();
}

static void EmitFormatReference(JsonOut& json) {
    json.beginObject("format_reference");
    json.beginObject("pak");
    json.field("footer_magic", "0x5A6F12E1");
    EmitStringArray(json, "footer_layout", {
        "EncryptionKeyGuid (v7+)", "bEncryptedIndex", "Magic", "Version",
        "IndexOffset", "IndexSize", "IndexHash (SHA-1)",
        "bIndexIsFrozen (v9 only)", "CompressionMethodName[5][32] (v8+)"});
    json.fieldUInt("footer_size_bytes_v11", 221);
    json.field("index_cipher", "AES-256-ECB");
    json.field("index_hash", "SHA-1 of the decrypted index bytes");
    json.field("entry_compression_method_index",
               "0 = None; on-disk name slot k maps to method index k+1");
    json.field("directory_index_type",
               "TMap<FString relative directory, TMap<FString file name, FPakEntryLocation>>");
    EmitStringArray(json, "source_files", {
        "Runtime/PakFile/Public/IPlatformFilePak.h",
        "Runtime/PakFile/Private/IPlatformFilePak.cpp"});
    json.endObject();
    json.beginObject("iostore");
    json.field("toc_magic", "-==--==--==--==-");
    json.field("toc_entry_layout", "FIoChunkId (12 bytes) + FIoOffsetAndLength (10 bytes) = 22 bytes");
    json.field("toc_entry_meta_layout", "FIoChunkHash (32 bytes) + FIoStoreTocEntryMetaFlags (1 byte) = 33 bytes");
    json.field("directory_index_cipher", "AES-256-ECB when the Encrypted container flag is set");
    json.field("directory_index_type",
               "FIoDirectoryIndexResource (mount point + directory tree + file entries + string table)");
    EmitStringArray(json, "source_files", {
        "Runtime/Core/Internal/IO/IoStore.h",
        "Runtime/Core/Private/IO/IoStore.cpp",
        "Runtime/Core/Private/IO/IoDirectoryIndex.cpp"});
    json.endObject();
    json.endObject();
}

static bool first_key_guid_is_zero(const std::vector<ArchiveReport>& archives);

std::string AESKeyResult::ToJson(const std::string& exe_path) const {
    std::string actual_exe = exe_path;
    if (actual_exe.empty()) {
        char buffer[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        actual_exe = buffer;
    }
    uint64_t exe_size = 0;
    std::error_code ec;
    if (fs::exists(actual_exe, ec)) exe_size = fs::file_size(actual_exe, ec);
    const std::string exe_name = fs::path(actual_exe).filename().string();

    SYSTEMTIME now;
    GetSystemTime(&now);
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);

    const bool encrypted = any_encryption || totals.encrypted_paks > 0 || totals.encrypted_utocs > 0;

    JsonOut json;
    json.beginObject();

    json.beginObject("generated_by");
    json.field("tool", "Avalonium AESKeyFinder");
    json.field("generated_at_utc", timestamp);
    json.endObject();

    json.beginObject("scan");
    json.field("mode", "in-process scan");
    json.field("timestamp_utc", timestamp);
    json.fieldInt("duration_ms", scan_duration_ms);
    json.fieldInt("candidates_tested", candidates_tested);
    json.fieldInt("report_build_ms", report_build_ms);
    json.endObject();

    json.beginObject("game");
    json.field("name", game_name.empty() ? exe_name : game_name);
    json.field("executable", exe_name);
    json.field("executable_path", actual_exe);
    json.fieldUInt("executable_size", exe_size);
    json.beginObject("version_info");
    if (!version.product_name.empty()) json.field("ProductName", version.product_name);
    if (!version.file_description.empty()) json.field("FileDescription", version.file_description);
    if (!version.company_name.empty()) json.field("CompanyName", version.company_name);
    if (!version.file_version.empty()) json.field("FileVersion", version.file_version);
    if (!version.product_version.empty()) json.field("ProductVersion", version.product_version);
    if (!version.original_filename.empty()) json.field("OriginalFilename", version.original_filename);
    if (!version.internal_name.empty()) json.field("InternalName", version.internal_name);
    if (!version.fixed_file_version.empty())
        json.field("fixed_file_version", version.fixed_file_version);
    if (!version.fixed_product_version.empty())
        json.field("fixed_product_version", version.fixed_product_version);
    json.endObject();
    json.endObject();

    json.beginObject("engine");
    json.field("family", "Unreal Engine");
    if (!engine_version.empty()) {
        json.field("version", engine_version);
        json.field("evidence", "VS_FIXEDFILEINFO product version");
    }
    json.endObject();

    json.beginObject("encryption");
    if (encrypted) {
        json.field("algorithm", "AES-256");
        json.field("mode", "ECB");
        json.fieldUInt("key_size_bits", 256);
        json.fieldUInt("block_size_bytes", 16);
    } else {
        json.field("algorithm", "none");
    }
    json.field("key_guid", FirstKeyGuid(archives));
    json.field("key_guid_meaning",
               encrypted
                   ? "All-zero GUID with encrypted containers means the default/embedded project key."
                   : "No container is encrypted; the zero GUID is unused.");
    json.fieldBool("pak_index_encrypted", totals.encrypted_paks > 0);
    json.fieldBool("iostore_directory_index_encrypted", totals.encrypted_utocs > 0);
    json.endObject();

    json.beginObject("key");
    json.fieldBool("found", key_found);
    if (key_found) {
        json.fieldBool("verified", key_verified);
        json.field("hex", key_hex);
        json.field("hex_spaced", key_spaced);
        json.field("base64", key_base64);
        json.field("c_array", key_c_array);
        json.field("unrealpak_arg", unrealpak_arg);

        json.beginObject("detection");
        json.field("method", source);
        json.fieldInt("candidates_tested", candidates_tested);
        if (source_address) json.field("runtime_address", std::format("0x{:X}", source_address));
        if (!source_module.empty()) json.field("module", source_module);
        if (source_module_offset_valid)
            json.field("module_offset", std::format("0x{:X}", source_module_offset));
        if (!source_section.empty()) json.field("section", source_section);
        json.field("on_disk_search", "not performed");
        json.endObject();

        json.beginObject("byte_statistics");
        json.fieldUInt("sample_bytes", 32);
        json.fieldUInt("unique_byte_values", static_cast<uint64_t>(std::set<uint8_t>(
            key_bytes, key_bytes + 32).size()));
        json.fieldRaw("shannon_entropy_bits_per_byte", std::format("{:.4f}", entropy));
        json.fieldRaw("sample_entropy_ceiling_bits_per_byte", "5.0");
        json.field("note", "Entropy of the 32 key bytes themselves; 5.0 is the maximum observable in a 32-byte sample and is not a key-strength estimate.");
        json.endObject();
    } else {
        json.fieldBool("required", encrypted);
        json.field("note", encrypted
            ? "Encrypted indexes are present but no key was found."
            : "No AES key is required: every discovered pak index and IoStore directory index is stored unencrypted.");
    }
    json.endObject();

    json.beginObject("verification");
    json.field("method",
               encrypted
                   ? "AES-256-ECB decryption followed by SHA-1 comparison against FPakInfo::IndexHash (pak) and structural parsing of FIoDirectoryIndex (IoStore)."
                   : "SHA-1 comparison against FPakInfo::IndexHash and structural parsing of FIoDirectoryIndex; the containers are stored unencrypted.");
    json.fieldBool("encrypted_containers", encrypted);
    json.beginObject("pak_primary_indexes");
    json.fieldInt("total", totals.pak_primary_total);
    json.fieldInt("verified", totals.pak_primary_verified);
    json.endObject();
    json.beginObject("pak_secondary_indexes");
    json.fieldInt("total", totals.pak_secondary_total);
    json.fieldInt("verified", totals.pak_secondary_verified);
    json.endObject();
    json.beginObject("iostore_directory_indexes");
    json.fieldInt("total", totals.utoc_directory_total);
    json.fieldInt("parsed", totals.utoc_directory_parsed);
    json.endObject();
    json.fieldInt("sample_entries_decoded", totals.sample_entries_decoded);
    json.fieldBool("all_checks_passed", totals.all_checks_passed);
    if (!totals.failures.empty()) {
        json.beginArray("failures");
        for (size_t i = 0; i < totals.failures.size() && i < 20; ++i)
            json.field(nullptr, totals.failures[i]);
        json.endArray();
    }
    json.endObject();

    json.beginObject("census");
    json.beginObject("containers");
    json.fieldInt("total", totals.containers_total);
    json.fieldInt("pak", totals.paks);
    json.fieldInt("utoc", totals.utocs);
    json.fieldInt("ucas", totals.ucas);
    json.endObject();
    json.beginObject("encrypted");
    json.fieldInt("paks", totals.encrypted_paks);
    json.fieldInt("utocs", totals.encrypted_utocs);
    json.fieldInt("ucas_chunk_data", totals.encrypted_ucas);
    json.endObject();
    json.fieldInt("pak_entries", totals.pak_entries);
    json.fieldInt("pak_indexed_files", totals.pak_indexed_files);
    json.fieldInt("iostore_chunks", totals.iostore_chunks);
    json.fieldInt("iostore_indexed_files", totals.iostore_indexed_files);
    json.fieldInt("iostore_non_file_chunks", totals.iostore_non_file_chunks);
    json.beginObject("bytes");
    json.fieldUInt("pak", totals.pak_bytes);
    json.fieldUInt("utoc", totals.utoc_bytes);
    json.fieldUInt("ucas", totals.ucas_bytes);
    json.fieldUInt("total", totals.pak_bytes + totals.utoc_bytes + totals.ucas_bytes);
    json.endObject();
    json.endObject();

    const ArchiveReport* target = nullptr;
    for (const auto& archive : archives) {
        if (archive.kind == "pak" && archive.primary_index.hash_verified) {
            target = &archive;
            break;
        }
    }
    if (!target) {
        for (const auto& archive : archives) {
            if (archive.kind == "iostore_toc" && archive.utoc_directory.decrypted) {
                target = &archive;
                break;
            }
        }
    }
    if (target) {
        json.beginObject("target_archive");
        json.field("file_name", target->file_name);
        json.field("role", target->kind == "pak"
            ? "first key-verified pak index"
            : "first parsed IoStore directory index");
        if (target->kind == "pak") {
            json.field("mount_point", target->mount_point);
            json.fieldInt("entry_count", target->entry_count);
        } else {
            json.field("mount_point", target->utoc_directory.mount_point);
            json.fieldInt("file_entries", target->utoc_directory.file_count);
        }
        json.endObject();
    }

    json.beginArray("archives");
    for (const auto& archive : archives) EmitArchive(json, archive);
    json.endArray();

    EmitFormatReference(json);

    json.beginArray("notes");
    if (key_found && source_module_offset_valid) {
        const std::string section_note = source_section.empty()
            ? std::string()
            : " (" + source_section + ")";
        json.field(nullptr, std::format(
            "The winning candidate was an in-memory buffer at {}+0x{:X}{}.",
            source_module, source_module_offset, section_note));
    }
    std::vector<std::string> empty_paks;
    for (const auto& archive : archives) {
        if (archive.kind == "pak" && archive.entry_count == 0 && archive.error.empty())
            empty_paks.push_back(archive.file_name);
    }
    if (!empty_paks.empty()) {
        std::string text = "Empty patch pak(s): ";
        for (size_t i = 0; i < empty_paks.size(); ++i) {
            if (i) text += ", ";
            text += empty_paks[i];
        }
        json.field(nullptr, text + ".");
    }
    if (totals.pak_primary_verified && totals.utoc_directory_parsed) {
        json.field(nullptr, std::format(
            "All {} pak primary indexes, {} pak secondary indexes, and {} IoStore directory indexes were verified.",
            totals.pak_primary_verified, totals.pak_secondary_verified,
            totals.utoc_directory_parsed));
    }
    if (!encrypted) {
        json.field(nullptr, "No container is encrypted; no AES key is required.");
    } else if (first_key_guid_is_zero(archives)) {
        json.field(nullptr, "The pak footers carry an all-zero encryption-key GUID; the containers use the default project key.");
    }
    if (!totals.failures.empty()) {
        json.field(nullptr, std::format("{} verification failure(s); see verification.failures.",
                                       totals.failures.size()));
    }
    json.endArray();

    json.endObject();
    return json.text + "\n";
}

static bool first_key_guid_is_zero(const std::vector<ArchiveReport>& archives) {
    for (const auto& archive : archives) {
        if (archive.kind == "pak" && archive.footer.valid) {
            for (uint8_t byte : archive.footer.encryption_key_guid) if (byte) return false;
        } else if (archive.kind == "iostore_toc" && archive.toc.valid) {
            for (uint8_t byte : archive.toc.encryption_key_guid) if (byte) return false;
        }
    }
    return true;
}

static void SaveJson(const AESKeyResult& res, const std::string& exe_path) {
    try {
        const std::string content = res.ToJson(exe_path);
        const auto path = fs::path(exe_path).parent_path() /
                          (fs::path(exe_path).stem().string() + "_AESKey.json");
        std::ofstream file(path);
        if (file) {
            file << content << '\n';
            file.close();
            char line[512];
            snprintf(line, sizeof(line), "[AESKeyFinder] JSON saved: %s\n", path.string().c_str());
            EngineDiscovery::WriteLogLine(line);
        }
        std::ofstream copy("AESKey.json");
        if (copy) copy << content << '\n';
    } catch (...) {}
}

struct ScanScope {
    std::string module;
    uintptr_t base = 0;
    size_t size = 0;
};

static bool TryCandidate(const uint8_t* candidate, uintptr_t addr,
                         const PakVerifyContext& vctx, AESKeyResult& result,
                         const char* source_name, const ScanScope& scope) {
    result.candidates_tested++;
    DecryptedIndexSummary summary;
    if (!PakVerifier::VerifyKey(candidate, vctx, summary)) return false;

    result.key_found = true;
    result.key_verified = true;
    memcpy(result.key_bytes, candidate, 32);
    result.key_hex = AESKeyFinder::FormatHex(candidate, 32, true);
    result.key_spaced = AESKeyFinder::FormatHex(candidate, 32, false);
    result.key_base64 = AESKeyFinder::FormatBase64(candidate, 32);
    result.key_c_array = AESKeyFinder::FormatCArray(candidate, 32);
    result.unrealpak_arg = "-aes=" + result.key_hex;
    result.entropy = AESKeyFinder::CalculateEntropy(candidate, 32);
    result.source = source_name;
    result.source_address = addr;
    result.source_module = scope.module;
    if (addr >= scope.base && addr < scope.base + scope.size) {
        result.source_module_offset = addr - scope.base;
        result.source_module_offset_valid = true;
        result.source_section = RvaToSectionName(scope.base, result.source_module_offset);
    }

    char log_buffer[768];
    snprintf(log_buffer, sizeof(log_buffer),
             "[AESKeyFinder] SUCCESS! Key FOUND & VERIFIED: %s\n"
             "[AESKeyFinder]   Base64: %s | Source: %s | Addr: 0x%llX | Candidates: %d\n",
             result.key_hex.c_str(), result.key_base64.c_str(), source_name,
             static_cast<unsigned long long>(addr), result.candidates_tested);
    EngineDiscovery::WriteLogLine(log_buffer);

    if (!summary.archive_name.empty()) {
        char detail[512];
        if (summary.is_iostore) {
            snprintf(detail, sizeof(detail),
                     "[AESKeyFinder] Verified against IoStore directory: %s | MountPoint: %s (Files: %d)\n",
                     summary.archive_name.c_str(), summary.mount_point.c_str(), summary.num_entries);
        } else {
            snprintf(detail, sizeof(detail),
                     "[AESKeyFinder] Verified against pak index: %s | MountPoint: %s (Entries: %d)\n",
                     summary.archive_name.c_str(), summary.mount_point.c_str(), summary.num_entries);
        }
        EngineDiscovery::WriteLogLine(detail);
    }
    return true;
}

static bool ScanCodeImmediates(uintptr_t code_base, size_t code_sz,
                               const PakVerifyContext& vctx, AESKeyResult& result,
                               const ScanScope& scope) {
    constexpr size_t CHUNK_SIZE = 64 * 1024;
    uint8_t chunk[CHUNK_SIZE];

    size_t remaining = code_sz;
    uintptr_t cur = code_base;

    while (remaining >= 32) {
        size_t to_read = (std::min)(remaining, CHUNK_SIZE);
        to_read = (to_read / 16) * 16;
        if (SafeMem_ReadSpanExact(reinterpret_cast<const void*>(cur), chunk, to_read) != to_read) {
            cur += to_read;
            remaining -= to_read;
            continue;
        }

        for (size_t i = 0; i + 40 <= to_read; ++i) {
            if (chunk[i] == 0x48 && (chunk[i + 1] >= 0xB8 && chunk[i + 1] <= 0xBF)) {
                uint8_t key[32] = {};
                int loads = 0;
                size_t pos = i;
                const size_t window_end = (std::min)(to_read, i + 80);

                while (loads < 4 && pos + 10 <= window_end) {
                    if (chunk[pos] == 0x48 && (chunk[pos + 1] >= 0xB8 && chunk[pos + 1] <= 0xBF)) {
                        memcpy(key + loads * 8, chunk + pos + 2, 8);
                        loads++;
                        pos += 10;
                    } else {
                        pos++;
                    }
                }

                if (loads == 4 && AESKeyFinder::LooksLikeKey(key)) {
                    if (TryCandidate(key, cur + i, vctx, result, "mov_imm64_code", scope))
                        return true;
                }
            }
        }

        for (size_t i = 0; i + 48 <= to_read; ++i) {
            if (chunk[i] == 0xC7) {
                uint8_t key[32] = {};
                int mov_count = 0;
                size_t pos = i;
                const size_t window_end = (std::min)(to_read, i + 96);

                while (mov_count < 8 && pos + 6 <= window_end) {
                    if (chunk[pos] == 0xC7) {
                        int imm_off = 2;
                        if (chunk[pos + 1] == 0x41 || chunk[pos + 1] == 0x43 || chunk[pos + 1] == 0x45 ||
                            chunk[pos + 1] == 0x46 || chunk[pos + 1] == 0x47) {
                            imm_off = 3;
                        } else if (chunk[pos + 1] == 0x44 && chunk[pos + 2] == 0x24) {
                            imm_off = 4;
                        } else if (chunk[pos + 1] == 0x81 || chunk[pos + 1] == 0x83 ||
                                   chunk[pos + 1] == 0x84 || chunk[pos + 1] == 0x85) {
                            imm_off = 6;
                        }
                        if (pos + imm_off + 4 <= window_end) {
                            uint32_t value = *reinterpret_cast<const uint32_t*>(chunk + pos + imm_off);
                            memcpy(key + mov_count * 4, &value, 4);
                            mov_count++;
                            pos += imm_off + 4;
                        } else {
                            break;
                        }
                    } else {
                        pos++;
                    }
                }

                if (mov_count == 8 && AESKeyFinder::LooksLikeKey(key)) {
                    if (TryCandidate(key, cur + i, vctx, result, "mov_imm32_code", scope))
                        return true;
                }
            }
        }

        cur += to_read;
        remaining -= to_read;
    }
    return false;
}

static bool ScanDataSections(uintptr_t base, size_t size,
                             const PakVerifyContext& vctx, AESKeyResult& result,
                             const ScanScope& scope) {
    constexpr size_t CHUNK_SIZE = 64 * 1024;
    uint8_t chunk[CHUNK_SIZE];

    size_t remaining = size;
    uintptr_t cur = base;

    auto hex_val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };

    auto b64_val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+' || c == '-') return 62;
        if (c == '/' || c == '_') return 63;
        return -1;
    };

    while (remaining >= 32) {
        size_t to_read = (std::min)(remaining, CHUNK_SIZE);
        to_read = (to_read / 16) * 16;
        if (SafeMem_ReadSpanExact(reinterpret_cast<const void*>(cur), chunk, to_read) != to_read) {
            cur += to_read;
            remaining -= to_read;
            continue;
        }

        for (size_t off = 0; off + 32 <= to_read; off += 16) {
            if (AESKeyFinder::LooksLikeKey(chunk + off)) {
                if (TryCandidate(chunk + off, cur + off, vctx, result, "data_raw_buffer", scope))
                    return true;
            }
        }

        for (size_t i = 0; i + 64 <= to_read; ++i) {
            if (i + 66 <= to_read && chunk[i] == '0' && (chunk[i + 1] == 'x' || chunk[i + 1] == 'X')) {
                uint8_t key[32] = {};
                bool valid = true;
                for (size_t j = 0; j < 32; ++j) {
                    const int h = hex_val(static_cast<char>(chunk[i + 2 + j * 2]));
                    const int l = hex_val(static_cast<char>(chunk[i + 2 + j * 2 + 1]));
                    if (h < 0 || l < 0) { valid = false; break; }
                    key[j] = static_cast<uint8_t>((h << 4) | l);
                }
                if (valid && AESKeyFinder::LooksLikeKey(key)) {
                    if (TryCandidate(key, cur + i, vctx, result, "hex_literal_0x", scope))
                        return true;
                }
            }

            {
                uint8_t key[32] = {};
                bool valid = true;
                for (size_t j = 0; j < 32; ++j) {
                    const int h = hex_val(static_cast<char>(chunk[i + j * 2]));
                    const int l = hex_val(static_cast<char>(chunk[i + j * 2 + 1]));
                    if (h < 0 || l < 0) { valid = false; break; }
                    key[j] = static_cast<uint8_t>((h << 4) | l);
                }
                if (valid && AESKeyFinder::LooksLikeKey(key)) {
                    if (TryCandidate(key, cur + i, vctx, result, "hex_literal_64char", scope))
                        return true;
                }
            }
        }

        for (size_t i = 0; i + 132 <= to_read; i += 2) {
            if (chunk[i] == '0' && chunk[i + 1] == 0 &&
                (chunk[i + 2] == 'x' || chunk[i + 2] == 'X') && chunk[i + 3] == 0) {
                uint8_t key[32] = {};
                bool valid = true;
                for (size_t j = 0; j < 32; ++j) {
                    const char ch1 = static_cast<char>(chunk[i + 4 + j * 4]);
                    const char ch2 = static_cast<char>(chunk[i + 4 + j * 4 + 2]);
                    if (chunk[i + 4 + j * 4 + 1] != 0 || chunk[i + 4 + j * 4 + 3] != 0) {
                        valid = false;
                        break;
                    }
                    const int h = hex_val(ch1);
                    const int l = hex_val(ch2);
                    if (h < 0 || l < 0) { valid = false; break; }
                    key[j] = static_cast<uint8_t>((h << 4) | l);
                }
                if (valid && AESKeyFinder::LooksLikeKey(key)) {
                    if (TryCandidate(key, cur + i, vctx, result, "utf16_hex_literal", scope))
                        return true;
                }
            }
        }

        for (size_t b_idx = 0; b_idx + 44 <= to_read; b_idx += 2) {
            bool valid_b64 = true;
            for (size_t j = 0; j < 43; ++j) {
                if (b64_val(static_cast<char>(chunk[b_idx + j])) < 0) { valid_b64 = false; break; }
            }
            if (!valid_b64) continue;

            uint8_t key[32] = {};
            size_t out_idx = 0;
            uint32_t value = 0;
            int bits = -8;

            for (size_t j = 0; j < 44 && out_idx < 32; ++j) {
                const char c = static_cast<char>(chunk[b_idx + j]);
                if (c == '=') break;
                const int v = b64_val(c);
                if (v < 0) break;
                value = (value << 6) | static_cast<uint32_t>(v);
                bits += 6;
                if (bits >= 0) {
                    key[out_idx++] = static_cast<uint8_t>((value >> bits) & 0xFF);
                    bits -= 8;
                }
            }

            if (out_idx == 32 && AESKeyFinder::LooksLikeKey(key)) {
                if (TryCandidate(key, cur + b_idx, vctx, result, "base64_string_data", scope))
                    return true;
            }
        }

        cur += to_read;
        remaining -= to_read;
    }
    return false;
}

AESKeyResult AESKeyFinder::Scan(const std::string& exe_path) {
    EngineDiscovery::BeginExclusiveLogSection("crypto");

    AESKeyResult result;
    std::string actual_exe = exe_path;
    if (actual_exe.empty()) {
        char buffer[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        actual_exe = buffer;
    }

    auto finish = [&]() -> AESKeyResult {
        {
            std::lock_guard<std::mutex> lock(s_Mutex);
            s_Result = result;
            s_Done = true;
        }
        EngineDiscovery::EndLogSection("crypto");
        return result;
    };

    result.version = QueryVersionInfo(actual_exe);
    result.game_name = !result.version.product_name.empty()
        ? result.version.product_name
        : fs::path(actual_exe).stem().string();
    if ((result.version.product_major == 4 || result.version.product_major == 5) &&
        result.version.product_minor < 100) {
        result.engine_version = std::format("{}.{}",
                                            result.version.product_major,
                                            result.version.product_minor);
    }

    EngineDiscovery::WriteLogLine("[AESKeyFinder] Starting dynamic in-process archive encryption scan...\n");

    const PakInfo pak_info = PakVerifier::CheckEncryption(actual_exe);
    if (!pak_info.found_pak) {
        EngineDiscovery::WriteLogLine("[AESKeyFinder] No pak/IoStore containers found. Aborting.\n");
        return finish();
    }

    result.any_encryption = pak_info.encrypted_pak_files > 0 || pak_info.encrypted_utoc_files > 0;
    PakVerifyContext vctx = PakVerifier::BuildVerifyContext(pak_info, actual_exe);

    const std::string primary_name = fs::path(pak_info.pak_path).filename().string();
    if (pak_info.is_utoc) {
        EngineDiscovery::WriteLogLine(std::format(
            "[AESKeyFinder] Target archive: {} | IoStore TOC v{} | encrypted={} | directory=0x{:X}/{}\n",
            primary_name, pak_info.toc.version,
            pak_info.is_encrypted ? "YES" : "NO",
            pak_info.toc.dir_offset, pak_info.toc.dir_size).c_str());
    } else {
        EngineDiscovery::WriteLogLine(std::format(
            "[AESKeyFinder] Target archive: {} | PAK v{} | encrypted={} | index=0x{:X}/{}\n",
            primary_name, pak_info.pak_version,
            pak_info.is_encrypted ? "YES" : "NO",
            pak_info.index_offset, pak_info.index_size).c_str());
    }

    if (result.any_encryption) {
        if (!vctx.ready) {
            EngineDiscovery::WriteLogLine("[AESKeyFinder] Could not read encrypted index blocks; key search skipped.\n");
        } else {
            HMODULE main_exe = GetModuleHandleA(nullptr);
            MODULEINFO main_info = {};
            uintptr_t exe_base = 0;
            size_t exe_size = 0;
            if (GetModuleInformation(GetCurrentProcess(), main_exe, &main_info, sizeof(main_info))) {
                exe_base = reinterpret_cast<uintptr_t>(main_info.lpBaseOfDll);
                exe_size = main_info.SizeOfImage;
            }

            fs::path exe_dir = fs::path(actual_exe).parent_path();
            fs::path game_root = exe_dir;
            for (int i = 0; i < 3 && game_root.has_parent_path(); ++i)
                game_root = game_root.parent_path();

            const auto start = std::chrono::high_resolution_clock::now();
            EngineDiscovery::WriteLogLine("[AESKeyFinder] Scanning main executable data & code sections...\n");

            const ScanScope main_scope{fs::path(actual_exe).filename().string(), exe_base, exe_size};
            uintptr_t cur = exe_base;
            const uintptr_t end = exe_base + exe_size;
            while (cur < end) {
                MEMORY_BASIC_INFORMATION mbi = {};
                if (VirtualQuery(reinterpret_cast<LPCVOID>(cur), &mbi, sizeof(mbi)) == 0) break;
                const uintptr_t region_base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                const size_t region_size = mbi.RegionSize;
                cur = region_base + region_size;

                if (mbi.State != MEM_COMMIT) continue;
                if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) continue;
                const DWORD protection = mbi.Protect & 0xFF;

                if (protection == PAGE_READONLY || protection == PAGE_READWRITE ||
                    protection == PAGE_WRITECOPY) {
                    if (ScanDataSections(region_base, region_size, vctx, result, main_scope))
                        break;
                } else if (protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE) {
                    if (ScanCodeImmediates(region_base, region_size, vctx, result, main_scope))
                        break;
                }
            }

            if (!result.key_found) {
                HMODULE modules[1024];
                DWORD needed = 0;
                if (EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed)) {
                    const size_t count = needed / sizeof(HMODULE);
                    for (size_t i = 0; i < count && !result.key_found; ++i) {
                        if (modules[i] == main_exe || modules[i] == g_Module) continue;
                        char module_path[MAX_PATH] = {};
                        GetModuleFileNameA(modules[i], module_path, MAX_PATH);
                        const std::string module_string = module_path;
                        const std::string game_root_string = game_root.string();
                        if (_strnicmp(module_string.c_str(), game_root_string.c_str(),
                                      game_root_string.size()) != 0)
                            continue;

                        MODULEINFO info = {};
                        if (!GetModuleInformation(GetCurrentProcess(), modules[i], &info, sizeof(info)))
                            continue;
                        const uintptr_t module_base = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
                        const size_t module_size = info.SizeOfImage;
                        const ScanScope scope{fs::path(module_string).filename().string(),
                                              module_base, module_size};

                        uintptr_t module_cur = module_base;
                        const uintptr_t module_end = module_base + module_size;
                        while (module_cur < module_end && !result.key_found) {
                            MEMORY_BASIC_INFORMATION mbi = {};
                            if (VirtualQuery(reinterpret_cast<LPCVOID>(module_cur), &mbi, sizeof(mbi)) == 0)
                                break;
                            const uintptr_t region_base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
                            const size_t region_size = mbi.RegionSize;
                            module_cur = region_base + region_size;

                            if (mbi.State != MEM_COMMIT) continue;
                            if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) continue;
                            const DWORD protection = mbi.Protect & 0xFF;

                            if (protection == PAGE_READONLY || protection == PAGE_READWRITE ||
                                protection == PAGE_WRITECOPY) {
                                if (ScanDataSections(region_base, region_size, vctx, result, scope))
                                    break;
                            } else if (protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE) {
                                if (ScanCodeImmediates(region_base, region_size, vctx, result, scope))
                                    break;
                            }
                        }
                    }
                }
            }

            const auto scan_end = std::chrono::high_resolution_clock::now();
            result.scan_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                scan_end - start).count();

            if (!result.key_found) {
                EngineDiscovery::WriteLogLine(std::format(
                    "[AESKeyFinder] Scan finished in {} ms. Candidates tested: {} | Key found: NO\n",
                    result.scan_duration_ms, result.candidates_tested).c_str());
            }
        }
    } else {
        EngineDiscovery::WriteLogLine("[AESKeyFinder] Container indexes are not encrypted; no AES key is required.\n");
    }

    const auto report_start = std::chrono::high_resolution_clock::now();
    PakVerifier::BuildArchiveReports(result.key_found ? result.key_bytes : nullptr,
                                     vctx.all_discovered_paks, ReportOptions{},
                                     result.archives, result.totals);
    result.report_build_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - report_start).count();

    SaveJson(result, actual_exe);
    return finish();
}

static DWORD WINAPI ScanThread(LPVOID param) {
    char* path = static_cast<char*>(param);
    std::string exe_path;
    if (path) { exe_path = path; delete[] path; }
    try {
        AESKeyFinder::Scan(exe_path);
    } catch (...) {
        EngineDiscovery::WriteLogLine("[AESKeyFinder] Exception caught in ScanThread!\n");
    }
    return 0;
}

void AESKeyFinder::ScanAsync(const std::string& exe_path) {
    char* copy = nullptr;
    if (!exe_path.empty()) {
        copy = new char[exe_path.size() + 1];
        memcpy(copy, exe_path.c_str(), exe_path.size() + 1);
    }
    HANDLE thread = CreateThread(nullptr, 0, ScanThread, copy, 0, nullptr);
    if (thread) CloseHandle(thread);
}

bool AESKeyFinder::IsScanFinished() { return s_Done.load(); }

AESKeyResult AESKeyFinder::GetResult() {
    std::lock_guard<std::mutex> lock(s_Mutex);
    return s_Result;
}

}
