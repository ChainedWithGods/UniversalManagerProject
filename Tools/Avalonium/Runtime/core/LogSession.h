#pragma once

#include <cstdint>
#include <string>

enum class LogLevel : int {
  Trace = 0,
  Debug = 1,
  Info = 2,
  Warning = 3,
  Error = 4,
  Fatal = 5,
};

const char *LogLevelTag(LogLevel level);

namespace LogSession {

bool SetSection(const char *id);
const char *CurrentSectionId();

void BeginExclusiveSection(const char *id);

void EndSection(const char *id = nullptr);

void SetThreadName(const char *name);

void SetMinLevel(LogLevel level);
LogLevel MinLevel();
void SetVerbose(bool verbose);
bool Verbose();

void SetFact(const char *key, const char *value);
void SetFact(const char *key, const std::string &value);
const char *GetFact(const char *key);

void SetResult(const char *outcome, const char *detail);

void SetPath(const std::string &path);
void Begin();
void End(const char *reason);

void Write(const char *tag, const std::string &text, LogLevel level);

LogLevel ClassifyMessage(const std::string &text);

}
