#pragma once

#include "PakVerifier.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Crypto {

struct ExeVersionInfo {
    bool valid = false;
    std::string product_name;
    std::string file_description;
    std::string company_name;
    std::string file_version;
    std::string product_version;
    std::string original_filename;
    std::string internal_name;
    std::string fixed_file_version;
    std::string fixed_product_version;
    uint32_t product_major = 0;
    uint32_t product_minor = 0;
};

struct AESKeyResult {
    bool key_found    = false;
    bool key_verified = false;
    uint8_t key_bytes[32] = {};

    std::string key_hex;
    std::string key_spaced;
    std::string key_base64;
    std::string key_c_array;
    std::string unrealpak_arg;

    std::string source;
    double entropy = 0.0;
    int    candidates_tested = 0;
    int64_t scan_duration_ms = 0;
    int64_t report_build_ms = 0;

    uintptr_t source_address = 0;
    std::string source_module;
    uint64_t source_module_offset = 0;
    std::string source_section;
    bool source_module_offset_valid = false;

    std::string game_name;
    std::string engine_version;
    ExeVersionInfo version;
    bool any_encryption = false;

    std::vector<ArchiveReport> archives;
    ReportTotals totals;

    std::string ToJson(const std::string& exe_path) const;
};

class AESKeyFinder {
public:

    static AESKeyResult Scan(const std::string& exe_path = "");

    static void ScanAsync(const std::string& exe_path = "");

    static bool        IsScanFinished();
    static AESKeyResult GetResult();

    static bool   LooksLikeKey(const uint8_t* ptr);
    static double CalculateEntropy(const uint8_t* ptr, size_t len);
    static std::string FormatHex(const uint8_t* data, size_t len, bool compact);
    static std::string FormatBase64(const uint8_t* data, size_t len);
    static std::string FormatCArray(const uint8_t* data, size_t len);
};

}
