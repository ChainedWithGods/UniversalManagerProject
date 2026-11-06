#pragma once

#include <chrono>
#include <string>

#include "LogSession.h"

namespace EngineDiscovery {

void SetLogPath(const std::string &path);

std::string GetLogPath();

bool IsVerboseLoggingEnabled();

void SetRuntimeVerboseLogging(bool enabled);

void ResetSdkLogTimeBaseline();

std::chrono::milliseconds GetSdkLogElapsed();

void WriteSdkLogLine(const char *line);

void WriteLogLine(const char *line);

void WriteLogLineEx(LogLevel level, const char *fmt, ...);

void BeginLogSession();

void EndLogSession(const char *reason);

void SetLogSection(const char *sectionId);

void BeginExclusiveLogSection(const char *sectionId);

void EndLogSection(const char *sectionId = nullptr);

void SetLogFact(const char *key, const char *value);
void SetLogFact(const char *key, const std::string &value);

void SetLogResult(const char *outcome, const char *detail);

void SetLogThreadName(const char *name);

}

void Log(const char *fmt, ...);
