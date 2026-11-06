#include <iostream>
#include <filesystem>
#include <string>
#include <optional>
#include <vector>
#include <algorithm>
#include <windows.h>
#include <conio.h>
#include "version_detector.hpp"
namespace fs = std::filesystem;

fs::path utf8_to_path(const std::string& utf8_str) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, nullptr, 0);
    if (wlen == 0) {
        return fs::path(utf8_str);
    }
    std::wstring wstr(wlen - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, wstr.data(), wlen);
    return fs::path(wstr);
}

void print_usage(const char* program_name) {
    std::cout << "UE Version Detector - Detects Unreal Engine version from game executables\n\n";
    std::cout << "Usage: " << program_name << " [path_to_exe] [options]\n\n";
    std::cout << "If no .exe is specified, auto-detects from the current directory.\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help       Show this help message\n";
    std::cout << "  -v, --verbose    Show detailed version information\n";
    std::cout << "  -j, --json       Output in JSON format\n";
    std::cout << "\nSupported: UE 4.0 - UE 5.8\n";
}
struct Options {
    bool verbose = false;
    bool json = false;
};
std::optional<std::string> find_exe_in_current_dir() {
    std::vector<std::string> candidates;

    try {
        for (const auto& entry : fs::directory_iterator(fs::current_path())) {
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext != ".exe") continue;

            std::string filename = entry.path().filename().string();
            std::string self_name = "ue_detector.exe";
            std::string lower_filename = filename;
            std::transform(lower_filename.begin(), lower_filename.end(), lower_filename.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower_filename == self_name) continue;

            std::wstring wpath = entry.path().wstring();
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (size_needed > 0) {
                std::string utf8_path(size_needed - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, utf8_path.data(), size_needed, nullptr, nullptr);

                if (filename.find("-Win64-Shipping") != std::string::npos ||
                    filename.find("-Win64-Debug") != std::string::npos ||
                    filename.find("-Win64") != std::string::npos) {
                    candidates.insert(candidates.begin(), utf8_path);
                } else {
                    candidates.push_back(utf8_path);
                }
            }
        }
    } catch (const std::exception&) {

        return std::nullopt;
    }
    if (!candidates.empty()) {
        return candidates.front();
    }
    return std::nullopt;
}
std::optional<Options> parse_args(int argc, char* argv[], std::string& exe_path) {
    Options opts;
    bool help_requested = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            help_requested = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "-j" || arg == "--json") {
            opts.json = true;
        } else if (exe_path.empty() && !arg.starts_with("-")) {
            exe_path = arg;
        }
    }
    if (help_requested) {
        return std::nullopt;
    }
    if (exe_path.empty()) {
        auto found = find_exe_in_current_dir();
        if (found) {
            exe_path = *found;
        } else {
            return std::nullopt;
        }
    }
    return opts;
}
bool is_running_interactively() {
    DWORD proc_ids[2] = {};
    DWORD proc_count = GetConsoleProcessList(proc_ids, 2);
    return proc_count <= 1;
}
int wmain(int argc, wchar_t* argv[]) {

    std::vector<std::string> args_utf8;
    for (int i = 0; i < argc; ++i) {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        if (size_needed > 0) {
            std::string utf8_arg(size_needed - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, utf8_arg.data(), size_needed, nullptr, nullptr);
            args_utf8.push_back(std::move(utf8_arg));
        } else {
            args_utf8.push_back("");
        }
    }

    std::vector<char*> args_ptrs;
    for (auto& arg : args_utf8) {
        args_ptrs.push_back(arg.data());
    }

    std::string exe_path;
    auto opts = parse_args(argc, args_ptrs.data(), exe_path);
    if (!opts) {
        print_usage(args_utf8[0].c_str());
        if (is_running_interactively()) {
            std::cout << "\nPress Enter to exit...";
            std::cin.get();
        }
        return 1;
    }
    if (!fs::exists(utf8_to_path(exe_path))) {
        std::cerr << "Error: File not found: " << exe_path << "\n";
        if (is_running_interactively()) {
            std::cerr << "\nPress Enter to exit...";
            std::cin.get();
        }
        return 1;
    }
    if (!fs::is_regular_file(utf8_to_path(exe_path))) {
        std::cerr << "Error: Not a file: " << exe_path << "\n";
        if (is_running_interactively()) {
            std::cerr << "\nPress Enter to exit...";
            std::cin.get();
        }
        return 1;
    }
    auto extension = utf8_to_path(exe_path).extension().string();
    if (extension != ".exe" && extension != ".EXE") {
        std::cerr << "Warning: File does not have .exe extension\n";
    }
    uedetect::VersionDetector detector;
    auto result = detector.detect(exe_path);
    if (!result) {
        std::cerr << "Error: Failed to detect UE version\n";
        if (is_running_interactively()) {
            std::cerr << "\nPress Enter to exit...";
            std::cin.get();
        }
        return 1;
    }
    if (opts->json) {
        std::cout << result->to_json();
    } else {
        if (opts->verbose) {
            std::cout << "UE Version: " << result->get_version_string() << "\n";
            std::cout << "Major: " << result->major << "\n";
            std::cout << "Minor: " << result->minor << "\n";
            std::cout << "Patch: " << result->patch << "\n";
            std::cout << "Changelist: " << result->changelist << "\n";
            std::cout << "Branch: " << result->branch << "\n";
            if (result->is_ue5) {
                std::cout << "Engine: Unreal Engine 5\n";
            } else {
                std::cout << "Engine: Unreal Engine 4\n";
            }
        } else {
            std::cout << result->get_version_string() << "\n";
        }
    }
    if (opts->json) {
        std::cout << "\n";
    }
    if (is_running_interactively()) {
        std::cout << "\nPress Enter to exit...";
        std::cin.get();
    }
    return 0;
}
