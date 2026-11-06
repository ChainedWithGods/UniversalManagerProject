#include "Logger.h"
#include "LogSession.h"
#include "RuntimeConfig.h"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <stdarg.h>

static std::string g_LogPath;
static bool g_LogConfigLoaded = false;
static bool g_LogPathProbed = false;
static bool g_VerboseLogging = false;
static bool g_RuntimeVerboseLogging = false;

namespace {

static std::string GetInjectorConfigPath() {
  char dllPath[MAX_PATH] = {};
  HMODULE hMod = NULL;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                          (LPCSTR)&g_LogPath, &hMod)) {
    return {};
  }

  GetModuleFileNameA(hMod, dllPath, MAX_PATH);
  FreeLibrary(hMod);

  std::string configPath(dllPath);
  size_t pos = configPath.find_last_of("\\/");
  if (pos == std::string::npos) {
    return {};
  }

  return configPath.substr(0, pos + 1) + "injector_config.json";
}

static void TryParseBooleanConfigLine(const std::string &line, const char *key,
                                      bool &outValue) {
  size_t keyPos = line.find(key);
  if (keyPos == std::string::npos)
    return;

  size_t colonPos = line.find(':', keyPos);
  if (colonPos == std::string::npos)
    return;

  std::string tail = line.substr(colonPos + 1);
  std::transform(tail.begin(), tail.end(), tail.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });

  if (tail.find("true") != std::string::npos)
    outValue = true;
  else if (tail.find("false") != std::string::npos)
    outValue = false;
}

static void LoadLogConfigIfNeeded() {
  if (g_LogConfigLoaded)
    return;
  g_LogConfigLoaded = true;

  std::string configPath = GetInjectorConfigPath();
  if (!configPath.empty()) {
    if (FILE *f = fopen(configPath.c_str(), "r")) {
      char buf[1024];
      while (fgets(buf, sizeof(buf), f)) {
        std::string line(buf);

        size_t logPathPos = line.find("\"log_path\"");
        if (logPathPos != std::string::npos && g_LogPath.empty()) {
          size_t start = line.find('"', logPathPos + 10);
          size_t end = (start != std::string::npos) ? line.find('"', start + 1)
                                                    : std::string::npos;
          if (start != std::string::npos && end != std::string::npos) {
            g_LogPath = line.substr(start + 1, end - start - 1);
          }
        }

        TryParseBooleanConfigLine(line, "\"verbose_log\"", g_VerboseLogging);
      }
      fclose(f);
    }
  }

  if (g_LogPath.empty()) {
    char dllPath[MAX_PATH] = {};
    HMODULE hMod = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           (LPCSTR)&g_LogPath, &hMod)) {
      GetModuleFileNameA(hMod, dllPath, MAX_PATH);
      FreeLibrary(hMod);
      std::string dir(dllPath);
      size_t pos = dir.find_last_of("\\/");
      if (pos != std::string::npos) {
        dir = dir.substr(0, pos + 1);
      }
      g_LogPath = dir + "Avalonium.log";
    } else {
      g_LogPath = "Avalonium.log";
    }
  }
}

static std::string GetTempFallbackLogPath() {
  char dllPath[MAX_PATH] = {};
  HMODULE hMod = NULL;
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                         (LPCSTR)&g_LogPath, &hMod)) {
    GetModuleFileNameA(hMod, dllPath, MAX_PATH);
    FreeLibrary(hMod);
    std::string dir(dllPath);
    size_t pos = dir.find_last_of("\\/");
    if (pos != std::string::npos) {
      dir = dir.substr(0, pos + 1);
    }
    return dir + "Avalonium.log";
  }
  return "Avalonium.log";
}

static void EnsureLogDirectoryExists(const std::string &filePath) {
  if (filePath.empty())
    return;

  std::error_code ec;
  const std::filesystem::path path(filePath);
  const std::filesystem::path parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
  }
}

static void EnsureWritableLogPath() {
  LoadLogConfigIfNeeded();
  if (g_LogPathProbed)
    return;
  EnsureLogDirectoryExists(g_LogPath);

  if (FILE *f = fopen(g_LogPath.c_str(), "a")) {
    fclose(f);
    g_LogPathProbed = true;
    return;
  }

  const std::string fallbackPath = GetTempFallbackLogPath();
  if (fallbackPath == g_LogPath) {
    g_LogPathProbed = true;
    return;
  }

  EnsureLogDirectoryExists(fallbackPath);
  if (FILE *f = fopen(fallbackPath.c_str(), "a")) {
    fclose(f);
    g_LogPath = fallbackPath;
  }
  g_LogPathProbed = true;
}

static bool IsVerboseLoggingActive() {
  return g_VerboseLogging || g_RuntimeVerboseLogging;
}

static bool ShouldSuppressNormalLogLine(const std::string &line) {
  if (IsVerboseLoggingActive() || line.empty())
    return false;

  static const char *kVerboseOnlyPatterns[] = {

      "[Discovery][Debug]",

      "[USMap] DEBUG:",
      "[Discovery] Complex array heuristic failed, trying naive pointer "
      "matching",
      "[Discovery] GUObjectArray address ",
      "[Discovery] TUObjectArray Candidate:",
      "[Discovery] Found 'Class' metaclass",
      "[Discovery] Found core class '",
      "[Discovery] ApplyFFieldLayoutFixups:",
      "[Discovery] FField chain found:",
      "[Discovery] FField candidate",
      "[Discovery] FFieldVariant",
      "[d3d12hook] Remembered candidate CommandQueue=",
      "[d3d12hook] Updated CommandQueue=",
      "[d3d12hook] Captured CommandQueue=",
      "[d3d12hook] Post-reinit",
      "[d3d12hook] Deferring render",
      "[d3d12hook] ImGui initialized. Deferring",
      "[d3d11hook] Render target restored",
      "[d3d11hook] Releasing render target",
      "[d3d11hook] Closing menu",
  };

  for (const char *pattern : kVerboseOnlyPatterns) {
    if (line.find(pattern) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}

namespace EngineDiscovery {

void SetLogPath(const std::string &path) {
  g_LogPath = path;
  g_LogPathProbed = false;
  LogSession::SetPath(path);
}

void SetLogSection(const char *sectionId) { LogSession::SetSection(sectionId); }

void BeginExclusiveLogSection(const char *sectionId) {
  LogSession::BeginExclusiveSection(sectionId);
}

void EndLogSection(const char *sectionId) {
  LogSession::EndSection(sectionId);
}

void SetLogFact(const char *key, const char *value) {
  LogSession::SetFact(key, value);
}

void SetLogFact(const char *key, const std::string &value) {
  LogSession::SetFact(key, value);
}

void SetLogResult(const char *outcome, const char *detail) {
  LogSession::SetResult(outcome, detail);
}

void SetLogThreadName(const char *name) { LogSession::SetThreadName(name); }

void BeginLogSession() {
  LoadLogConfigIfNeeded();
  EnsureWritableLogPath();
  LogSession::SetPath(g_LogPath);
  LogSession::SetVerbose(IsVerboseLoggingActive());
  LogSession::SetMinLevel(LogLevel::Info);
  LogSession::Begin();
}

void EndLogSession(const char *reason) { LogSession::End(reason); }

void SetRuntimeVerboseLogging(bool enabled) {
  g_RuntimeVerboseLogging = enabled;
  LogSession::SetVerbose(IsVerboseLoggingActive());
}

std::string GetLogPath() {
  LoadLogConfigIfNeeded();
  EnsureWritableLogPath();
  return g_LogPath;
}

bool IsVerboseLoggingEnabled() {
  LoadLogConfigIfNeeded();
  return IsVerboseLoggingActive();
}

void WriteLogLine(const char *line) {
  if (!line || !*line)
    return;

  LoadLogConfigIfNeeded();
  std::string text(line);
  if (ShouldSuppressNormalLogLine(text))
    return;

  LogSession::Write(nullptr, text, LogSession::ClassifyMessage(text));
}

void WriteLogLineEx(LogLevel level, const char *fmt, ...) {
  if (!fmt || !*fmt)
    return;

  LoadLogConfigIfNeeded();

  char buf[4096] = {};
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  std::string text(buf);
  if (ShouldSuppressNormalLogLine(text))
    return;

  LogSession::Write(nullptr, text, level);
}

static std::mutex g_SdkLogStartTimeMutex;
static std::chrono::steady_clock::time_point g_SdkLogStartTime{};

void ResetSdkLogTimeBaseline() {
  std::lock_guard<std::mutex> lock(g_SdkLogStartTimeMutex);
  g_SdkLogStartTime = std::chrono::steady_clock::now();
}

std::chrono::milliseconds GetSdkLogElapsed() {
  std::lock_guard<std::mutex> lock(g_SdkLogStartTimeMutex);
  if (g_SdkLogStartTime.time_since_epoch().count() == 0) {
    return std::chrono::milliseconds(0);
  }
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - g_SdkLogStartTime);
}

void WriteSdkLogLine(const char *line) {
  if (line == nullptr || line[0] == '\0') {
    return;
  }

  std::string text(line);
  LogSession::Write(nullptr, text, LogSession::ClassifyMessage(text));
}

}

void Log(const char *fmt, ...) {
  char buf[4096];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  EngineDiscovery::WriteLogLine(buf);
}
