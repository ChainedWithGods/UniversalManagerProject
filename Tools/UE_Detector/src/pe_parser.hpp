#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <windows.h>
#include <dbghelp.h>
namespace uedetect {
#pragma pack(push, 1)
struct FileVersionInfo {
    uint32_t dwSignature;
    uint32_t dwStrucVersion;
    uint32_t dwFileVersionMS;
    uint32_t dwFileVersionLS;
    uint32_t dwProductVersionMS;
    uint32_t dwProductVersionLS;
    uint32_t dwFileFlagsMask;
    uint32_t dwFileFlags;
    uint32_t dwFileOS;
    uint32_t dwFileType;
    uint32_t dwFileSubtype;
    uint32_t dwFileDateMS;
    uint32_t dwFileDateLS;
};
#pragma pack(pop)
struct VersionStringInfo {
    std::string company_name;
    std::string file_description;
    std::string file_version;
    std::string internal_name;
    std::string legal_copyright;
    std::string original_filename;
    std::string product_name;
    std::string product_version;
    std::string comments;
};
class PEParser {
public:
    ~PEParser();
    bool open(const std::string& path);
    void close();
    bool is_open() const;
    std::optional<FileVersionInfo> get_version_info();
    std::optional<VersionStringInfo> get_version_strings();
    std::vector<std::string> scan_for_version_patterns();
    std::optional<std::string> get_exported_version_data();
private:
    std::optional<std::string> query_version_value(void* version_data, const wchar_t* value_name);
    std::vector<uint8_t> read_file_section(uint32_t rva, uint32_t size);
    HANDLE file_handle_ = INVALID_HANDLE_VALUE;
    HANDLE mapping_handle_ = nullptr;
    void* mapped_base_ = nullptr;
    size_t file_size_ = 0;
};
}
