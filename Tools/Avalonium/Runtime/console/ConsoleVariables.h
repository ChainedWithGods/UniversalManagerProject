#pragma once

#include "../core/EngineDiscovery.h"

#include <cstdint>

namespace ConsoleEnabler {

struct ConsoleObjectSlots {
  int help = -1;
  int flags = -1;
  int setFlags = -1;
  int asVariable = -1;
  int isBool = -1;
  int isInt = -1;
  int isFloat = -1;
  int getBool = -1;
  int getInt = -1;
  int getFloat = -1;
  int getString = -1;
};

enum class ConsoleValueKind {
  Command,
  Value,
  Unavailable,
  Faulted,
};

enum class ConsoleValueType {
  None,
  Bool,
  Int,
  Float,
  String,
};

struct ConsoleValueResult {
  ConsoleValueKind kind = ConsoleValueKind::Unavailable;
  ConsoleValueType type = ConsoleValueType::None;
  char value[512] = {0};
};

ConsoleObjectSlots ResolveConsoleObjectSlots(const EngineState &state, void *consoleObject);

ConsoleValueResult ReadConsoleValue(void *consoleObject, const ConsoleObjectSlots &slots);

void DumpConsoleVariables(const EngineState &state, void *dataPtr, uint32_t *allocationFlags,
                          int32_t arrayNum, int32_t stride);

}
