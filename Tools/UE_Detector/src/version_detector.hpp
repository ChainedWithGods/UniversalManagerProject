#pragma once
#include <string>
#include <optional>
#include <format>
namespace uedetect {
struct VersionInfo {
    int major = 0;
    int minor = 0;
    int patch = 0;
    int changelist = 0;
    std::string branch;
    bool is_ue5 = false;
    std::string get_version_string() const {
        if (is_valid()) {
            if (changelist > 0) {
                return std::format("UE {}.{}.{}-{}+{}",
                    is_ue5 ? "5" : "4", minor, patch, changelist, branch);
            }
            return std::format("UE {}.{}.{}",
                is_ue5 ? "5" : "4", minor, patch);
        }
        return "Unknown";
    }
    bool is_valid() const {
        return major > 0 && minor >= 0;
    }
    bool is_supported() const {
        if (!is_valid()) return false;
        if (is_ue5) {
            return major == 5 && minor >= 0 && minor <= 7;
        } else {
            return major == 4 && minor >= 0 && minor <= 27;
        }
    }
    std::string to_json() const {
        return std::format("{{\"major\":{},\"minor\":{},\"patch\":{},\"changelist\":{},\"branch\":\"{}\",\"is_ue5\":{},\"version_string\":\"{}\"}}",
            major, minor, patch, changelist, branch, is_ue5 ? "true" : "false", get_version_string());
    }
};
class VersionDetector {
public:
    std::optional<VersionInfo> detect(const std::string& exe_path);
private:
    std::optional<VersionInfo> parse_version_from_string(const std::string& str);
    std::optional<VersionInfo> detect_from_pe_resources(const std::string& exe_path);
    std::optional<VersionInfo> detect_from_binary_scan(const std::string& exe_path);
    std::optional<VersionInfo> detect_from_exported_data(const std::string& exe_path);
};
}
