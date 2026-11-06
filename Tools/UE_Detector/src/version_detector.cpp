#include "version_detector.hpp"
#include "pe_parser.hpp"
#include <regex>
#include <fstream>
#include <sstream>
namespace uedetect {
std::optional<VersionInfo> VersionDetector::detect(const std::string& exe_path) {
    auto result = detect_from_pe_resources(exe_path);
    if (result && result->is_valid()) {
        return result;
    }
    result = detect_from_binary_scan(exe_path);
    if (result && result->is_valid()) {
        return result;
    }
    result = detect_from_exported_data(exe_path);
    if (result && result->is_valid()) {
        return result;
    }
    return std::nullopt;
}
std::optional<VersionInfo> VersionDetector::detect_from_pe_resources(const std::string& exe_path) {
    PEParser parser;
    if (!parser.open(exe_path)) {
        return std::nullopt;
    }
    auto strings = parser.get_version_strings();
    if (!strings) {
        return std::nullopt;
    }
    VersionInfo info;
    if (!strings->product_name.empty()) {
        auto parsed = parse_version_from_string(strings->product_name);
        if (parsed) {
            return parsed;
        }
    }
    if (!strings->product_version.empty()) {
        auto parsed = parse_version_from_string(strings->product_version);
        if (parsed) {
            return parsed;
        }
    }
    if (!strings->file_version.empty()) {
        auto parsed = parse_version_from_string(strings->file_version);
        if (parsed) {
            return parsed;
        }
    }
    if (!strings->file_description.empty()) {
        auto parsed = parse_version_from_string(strings->file_description);
        if (parsed) {
            return parsed;
        }
    }
    auto fixed_info = parser.get_version_info();
    if (fixed_info) {
        uint32_t major = HIWORD(fixed_info->dwProductVersionMS);
        uint32_t minor = LOWORD(fixed_info->dwProductVersionMS);
        uint32_t patch = HIWORD(fixed_info->dwProductVersionLS);
        if (major == 4 || major == 5) {
            info.major = static_cast<int>(major);
            info.minor = static_cast<int>(minor);
            info.patch = static_cast<int>(patch);
            info.is_ue5 = (major == 5);
            return info;
        }
    }
    return std::nullopt;
}
std::optional<VersionInfo> VersionDetector::detect_from_binary_scan(const std::string& exe_path) {
    PEParser parser;
    if (!parser.open(exe_path)) {
        return std::nullopt;
    }
    auto patterns = parser.scan_for_version_patterns();
    for (const auto& pattern : patterns) {
        auto parsed = parse_version_from_string(pattern);
        if (parsed && parsed->is_valid()) {
            return parsed;
        }
    }
    return std::nullopt;
}
std::optional<VersionInfo> VersionDetector::detect_from_exported_data(const std::string& exe_path) {
    PEParser parser;
    if (!parser.open(exe_path)) {
        return std::nullopt;
    }
    auto exported = parser.get_exported_version_data();
    if (exported) {
        auto parsed = parse_version_from_string(*exported);
        if (parsed && parsed->is_valid()) {
            return parsed;
        }
    }
    return std::nullopt;
}
std::optional<VersionInfo> VersionDetector::parse_version_from_string(const std::string& str) {
    VersionInfo info;
    std::regex ue4_regex(R"((?:UE[-_]?|Unreal Engine\s+)(4)\.?(\d+)(?:\.(\d+))?)");
    std::regex ue5_regex(R"((?:UE[-_]?|Unreal Engine\s+)(5)\.?(\d+)(?:\.(\d+))?)");
    std::regex standalone_regex(R"(\b(4|5)\.(\d+)(?:\.(\d+))?)");

    std::regex ue_release_regex(R"(\+UE(4|5)\+Release-(\d+)\.(\d+))");
    std::smatch match;
    if (std::regex_search(str, match, ue_release_regex)) {
        info.major = std::stoi(match[1].str());
        info.minor = std::stoi(match[3].str());
        info.patch = 0;
        info.is_ue5 = (info.major == 5);
        if (info.is_ue5) {
            if (info.minor > 7) return std::nullopt;
        } else {
            if (info.minor > 27) return std::nullopt;
        }
        return info;
    }
    if (std::regex_search(str, match, ue5_regex)) {
        info.major = 5;
        info.minor = std::stoi(match[2].str());
        info.patch = match[3].matched ? std::stoi(match[3].str()) : 0;
        info.is_ue5 = true;
        if (info.minor > 7) {
            return std::nullopt;
        }
        return info;
    }
    if (std::regex_search(str, match, ue4_regex)) {
        info.major = 4;
        info.minor = std::stoi(match[2].str());
        info.patch = match[3].matched ? std::stoi(match[3].str()) : 0;
        info.is_ue5 = false;
        if (info.minor > 27) {
            return std::nullopt;
        }
        return info;
    }
    if (std::regex_search(str, match, standalone_regex)) {
        info.major = std::stoi(match[1].str());
        info.minor = std::stoi(match[2].str());
        info.patch = match[3].matched ? std::stoi(match[3].str()) : 0;
        info.is_ue5 = (info.major == 5);
        if (info.is_ue5) {
            if (info.minor > 7) return std::nullopt;
        } else {
            if (info.minor > 27) return std::nullopt;
        }
        return info;
    }
    std::regex changelist_regex(R"((?:CL|Changelist)[:\s-]*(\d+))");
    if (std::regex_search(str, match, changelist_regex)) {
        info.changelist = std::stoi(match[1].str());
    }
    std::regex branch_regex(R"(\+([\w\-\.]+))");
    if (std::regex_search(str, match, branch_regex)) {
        info.branch = match[1].str();
    }
    return std::nullopt;
}
}
