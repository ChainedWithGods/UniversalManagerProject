#include "pe_parser.hpp"
#include <cstring>
#include <algorithm>
#include <format>
#include <regex>
namespace uedetect {
PEParser::~PEParser() {
    close();
}
bool PEParser::open(const std::string& path) {
    close();

    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wlen == 0) {
        return false;
    }

    std::wstring wpath(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);

    file_handle_ = CreateFileW(
        wpath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (file_handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file_handle_, &file_size)) {
        close();
        return false;
    }
    file_size_ = static_cast<size_t>(file_size.QuadPart);
    mapping_handle_ = CreateFileMapping(
        file_handle_,
        nullptr,
        PAGE_READONLY,
        0,
        0,
        nullptr
    );
    if (!mapping_handle_) {
        close();
        return false;
    }
    mapped_base_ = MapViewOfFile(
        mapping_handle_,
        FILE_MAP_READ,
        0,
        0,
        0
    );
    if (!mapped_base_) {
        close();
        return false;
    }
    auto* dos_header = static_cast<IMAGE_DOS_HEADER*>(mapped_base_);
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
        close();
        return false;
    }
    auto* nt_headers = reinterpret_cast<IMAGE_NT_HEADERS*>(
        static_cast<uint8_t*>(mapped_base_) + dos_header->e_lfanew
    );
    if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
        close();
        return false;
    }
    return true;
}
void PEParser::close() {
    if (mapped_base_) {
        UnmapViewOfFile(mapped_base_);
        mapped_base_ = nullptr;
    }
    if (mapping_handle_) {
        CloseHandle(mapping_handle_);
        mapping_handle_ = nullptr;
    }
    if (file_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(file_handle_);
        file_handle_ = INVALID_HANDLE_VALUE;
    }
    file_size_ = 0;
}
bool PEParser::is_open() const {
    return mapped_base_ != nullptr;
}
std::optional<FileVersionInfo> PEParser::get_version_info() {
    if (!is_open()) return std::nullopt;
    auto* dos_header = static_cast<IMAGE_DOS_HEADER*>(mapped_base_);
    auto* nt_headers = reinterpret_cast<IMAGE_NT_HEADERS*>(
        static_cast<uint8_t*>(mapped_base_) + dos_header->e_lfanew
    );
    auto* data_dir = &nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
    if (data_dir->Size == 0) {
        return std::nullopt;
    }
    auto* sections = IMAGE_FIRST_SECTION(nt_headers);
    uint32_t resource_rva = data_dir->VirtualAddress;
    for (int i = 0; i < nt_headers->FileHeader.NumberOfSections; ++i) {
        auto& section = sections[i];
        if (resource_rva >= section.VirtualAddress &&
            resource_rva < section.VirtualAddress + section.Misc.VirtualSize) {
            auto section_data = read_file_section(
                section.PointerToRawData,
                section.SizeOfRawData
            );
            if (section_data.empty()) break;
            VS_FIXEDFILEINFO* vsffi = nullptr;
            UINT vsffi_len = 0;
            wchar_t file_path[MAX_PATH];
            DWORD path_len = GetFinalPathNameByHandleW(file_handle_, file_path, MAX_PATH, 0);
            if (path_len > 0 && path_len < MAX_PATH) {
                const wchar_t* actual_path = file_path;
                if (wcsncmp(file_path, L"\\\\?\\", 4) == 0) {
                    actual_path = file_path + 4;
                }
                DWORD handle;
                DWORD size = GetFileVersionInfoSizeW(actual_path, &handle);
                if (size > 0) {
                    std::vector<uint8_t> version_data(size);
                    if (GetFileVersionInfoW(actual_path, handle, size, version_data.data())) {
                        if (VerQueryValueA(version_data.data(), "\\", reinterpret_cast<LPVOID*>(&vsffi), &vsffi_len)) {
                            FileVersionInfo info;
                            info.dwSignature = vsffi->dwSignature;
                            info.dwFileVersionMS = vsffi->dwFileVersionMS;
                            info.dwFileVersionLS = vsffi->dwFileVersionLS;
                            info.dwProductVersionMS = vsffi->dwProductVersionMS;
                            info.dwProductVersionLS = vsffi->dwProductVersionLS;
                            return info;
                        }
                    }
                }
            }
            break;
        }
    }
    return std::nullopt;
}
std::optional<VersionStringInfo> PEParser::get_version_strings() {
    if (!is_open()) return std::nullopt;
    wchar_t file_path[MAX_PATH];
    DWORD path_len = GetFinalPathNameByHandleW(file_handle_, file_path, MAX_PATH, 0);
    if (path_len == 0 || path_len >= MAX_PATH) {
        return std::nullopt;
    }
    const wchar_t* actual_path = file_path;
    if (wcsncmp(file_path, L"\\\\?\\", 4) == 0) {
        actual_path = file_path + 4;
    }
    DWORD handle;
    DWORD size = GetFileVersionInfoSizeW(actual_path, &handle);
    if (size == 0) {
        return std::nullopt;
    }
    std::vector<uint8_t> version_data(size);
    if (!GetFileVersionInfoW(actual_path, handle, size, version_data.data())) {
        return std::nullopt;
    }
    VersionStringInfo info;
    struct QueryInfo {
        const wchar_t* name;
        std::string VersionStringInfo::* member;
    };
    QueryInfo queries[] = {
        { L"CompanyName", &VersionStringInfo::company_name },
        { L"FileDescription", &VersionStringInfo::file_description },
        { L"FileVersion", &VersionStringInfo::file_version },
        { L"InternalName", &VersionStringInfo::internal_name },
        { L"LegalCopyright", &VersionStringInfo::legal_copyright },
        { L"OriginalFilename", &VersionStringInfo::original_filename },
        { L"ProductName", &VersionStringInfo::product_name },
        { L"ProductVersion", &VersionStringInfo::product_version },
        { L"Comments", &VersionStringInfo::comments },
    };
    for (const auto& query : queries) {
        auto result = query_version_value(version_data.data(), query.name);
        if (result) {
            info.*query.member = *result;
        }
    }
    return info;
}
std::optional<std::string> PEParser::query_version_value(void* version_data, const wchar_t* value_name) {
    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *lpTranslate;
    UINT cbTranslate = 0;
    if (!VerQueryValueW(version_data, L"\\VarFileInfo\\Translation",
                        reinterpret_cast<LPVOID*>(&lpTranslate), &cbTranslate)) {
        return std::nullopt;
    }
    for (UINT i = 0; i < (cbTranslate / sizeof(LANGANDCODEPAGE)); i++) {
        wchar_t sub_block[256];
        swprintf_s(sub_block, L"\\StringFileInfo\\%04x%04x\\%s",
                   lpTranslate[i].wLanguage, lpTranslate[i].wCodePage, value_name);
        LPVOID value = nullptr;
        UINT len = 0;
        if (VerQueryValueW(version_data, sub_block, &value, &len)) {
            if (len > 1) {
                int size_needed = WideCharToMultiByte(CP_UTF8, 0,
                    static_cast<wchar_t*>(value), -1, nullptr, 0, nullptr, nullptr);
                std::string result(size_needed - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, static_cast<wchar_t*>(value), -1,
                    result.data(), size_needed, nullptr, nullptr);
                return result;
            }
        }
    }
    return std::nullopt;
}
std::vector<uint8_t> PEParser::read_file_section(uint32_t offset, uint32_t size) {
    std::vector<uint8_t> data(size);
    LARGE_INTEGER pos;
    pos.QuadPart = offset;
    if (!SetFilePointerEx(file_handle_, pos, nullptr, FILE_BEGIN)) {
        return {};
    }
    DWORD bytes_read = 0;
    if (!ReadFile(file_handle_, data.data(), size, &bytes_read, nullptr)) {
        return {};
    }
    data.resize(bytes_read);
    return data;
}
std::vector<std::string> PEParser::scan_for_version_patterns() {
    std::vector<std::string> matches;
    if (!is_open() || file_size_ == 0) {
        return matches;
    }

    constexpr size_t MAX_SCAN_SIZE = 512 * 1024 * 1024;
    size_t scan_size = (std::min)(file_size_, MAX_SCAN_SIZE);
    auto file_data = read_file_section(0, static_cast<uint32_t>(scan_size));
    if (file_data.empty()) {
        return matches;
    }
    std::regex version_regex(R"((UE[-_]?[45]\.[\d]+(?:\.[\d]+)?(?:\.[\d]+)?))");
    std::regex build_version_regex(R"((Build:\s*\d+|CL\s*\d+|Changelist:\s*\d+))");
    std::regex engine_version_regex(R"((?:Unreal Engine|UE)[\s_-]?(4|5)\.([\d]+))");
    std::string content(file_data.begin(), file_data.end());
    std::sregex_iterator iter(content.begin(), content.end(), version_regex);
    std::sregex_iterator end;
    for (; iter != end; ++iter) {
        matches.push_back(iter->str());
    }
    const char* ue_patterns[] = {
        "UE4",
        "UE5",
        "Unreal Engine 4",
        "Unreal Engine 5",
        "/Engine/",
        "UE_4.",
        "UE_5.",

        "++UE4+Release-",
        "++UE5+Release-",
        "+UE4+Release-",
        "+UE5+Release-"
    };
    for (const auto& pattern : ue_patterns) {
        size_t pos = content.find(pattern);
        while (pos != std::string::npos) {
            size_t end_pos = pos + strlen(pattern);
            size_t context_start = (pos > 20) ? pos - 20 : 0;
            size_t context_end = (std::min)(end_pos + 20, content.size());
            std::string context = content.substr(context_start, context_end - context_start);
            context.erase(std::remove_if(context.begin(), context.end(),
                [](char c) { return c == '\0' || (c < 32 && c != '\n' && c != '\r'); }),
                context.end());
            if (!context.empty()) {
                matches.push_back(context);
            }
            pos = content.find(pattern, pos + 1);
        }
    }
    std::sort(matches.begin(), matches.end());
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
    return matches;
}
std::optional<std::string> PEParser::get_exported_version_data() {
    if (!is_open()) return std::nullopt;
    auto* dos_header = static_cast<IMAGE_DOS_HEADER*>(mapped_base_);
    auto* nt_headers = reinterpret_cast<IMAGE_NT_HEADERS*>(
        static_cast<uint8_t*>(mapped_base_) + dos_header->e_lfanew
    );
    auto* data_dir = &nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (data_dir->Size == 0) {
        return std::nullopt;
    }
    auto* sections = IMAGE_FIRST_SECTION(nt_headers);
    uint32_t export_rva = data_dir->VirtualAddress;
    for (int i = 0; i < nt_headers->FileHeader.NumberOfSections; ++i) {
        auto& section = sections[i];
        if (export_rva >= section.VirtualAddress &&
            export_rva < section.VirtualAddress + section.Misc.VirtualSize) {
            uint32_t file_offset = section.PointerToRawData + (export_rva - section.VirtualAddress);
            uint32_t export_section_size = static_cast<uint32_t>(std::min<uint64_t>(data_dir->Size, 65536u));
            auto export_data = read_file_section(file_offset, export_section_size);
            if (export_data.size() < sizeof(IMAGE_EXPORT_DIRECTORY)) {
                return std::nullopt;
            }
            auto* export_dir = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(export_data.data());
            uint32_t name_rva = export_dir->Name;
            if (name_rva > 0) {
                uint32_t name_offset = section.PointerToRawData + (name_rva - section.VirtualAddress);
                auto name_data = read_file_section(name_offset, 256);
                if (!name_data.empty()) {
                    std::string module_name(reinterpret_cast<char*>(name_data.data()));
                    if (module_name.find("UE") != std::string::npos ||
                        module_name.find("Unreal") != std::string::npos) {
                        return module_name;
                    }
                }
            }
            break;
        }
    }
    return std::nullopt;
}
}
