#include "ConsoleVariables.h"
#include "ConsoleInternal.h"

#include "../core/RuntimeConfig.h"
#include "../utils/SafeMemory.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ConsoleEnabler {

namespace {

constexpr uint32_t kECVF_Cheat = 0x00000001;
constexpr uint32_t kECVF_ReadOnly = 0x00000004;
constexpr uint32_t kECVF_Unregistered = 0x00000008;
constexpr uint32_t kECVF_CreatedFromIni = 0x00000010;
constexpr uint32_t kECVF_RenderThreadSafe = 0x00000020;
constexpr uint32_t kECVF_Scalability = 0x00000040;
constexpr uint32_t kECVF_ScalabilityGroup = 0x00000080;
constexpr uint32_t kECVF_Preview = 0x00000100;
constexpr uint32_t kECVF_GeneralShaderChange = 0x00000200;
constexpr uint32_t kECVF_MobileShaderChange = 0x00000400;
constexpr uint32_t kECVF_DesktopShaderChange = 0x00000800;
constexpr uint32_t kECVF_ExcludeFromPreview = 0x00001000;
constexpr uint32_t kECVF_SaveForNextBoot = 0x00002000;
constexpr uint32_t kECVF_SetByMask = 0xff000000;

const char *SetByName(uint32_t setBy) {
  switch (setBy) {
    case 0x00: return "Constructor";
    case 0x02: return "Scalability";
    case 0x03: return "GameSetting";
    case 0x04: return "ProjectSetting";
    case 0x05: return "SystemSettingsIni";
    case 0x06: return "PluginLowPriority";
    case 0x07: return "DeviceProfile";
    case 0x08: return "PluginHighPriority";
    case 0x09: return "GameOverride";
    case 0x0A: return "ConsoleVariablesIni";
    case 0x0B: return "Hotfix";
    case 0x0C: return "Preview";
    case 0x0D: return "Commandline";
    case 0x0E: return "Code";
    case 0x0F: return "Temp";
    case 0x10: return "Console";
    default: return nullptr;
  }
}

std::string ConsoleFlagNames(uint32_t flags) {
  std::string text;
  auto append = [&text](const char *name) {
    if (!text.empty()) text += ", ";
    text += name;
  };

  if (flags & kECVF_Cheat) append("Cheat");
  if (flags & kECVF_ReadOnly) append("ReadOnly");
  if (flags & kECVF_Unregistered) append("Unregistered");
  if (flags & kECVF_CreatedFromIni) append("CreatedFromIni");
  if (flags & kECVF_RenderThreadSafe) append("RenderThreadSafe");
  if (flags & kECVF_Scalability) append("Scalability");
  if (flags & kECVF_ScalabilityGroup) append("ScalabilityGroup");
  if (flags & kECVF_Preview) append("Preview");
  if (flags & kECVF_GeneralShaderChange) append("GeneralShaderChange");
  if (flags & kECVF_MobileShaderChange) append("MobileShaderChange");
  if (flags & kECVF_DesktopShaderChange) append("DesktopShaderChange");
  if (flags & kECVF_ExcludeFromPreview) append("ExcludeFromPreview");
  if (flags & kECVF_SaveForNextBoot) append("SaveForNextBoot");

  const char *source = SetByName((flags & kECVF_SetByMask) >> 24);
  if (source) {
    if (!text.empty()) text += ", ";
    text += "SetBy:";
    text += source;
  }
  return text;
}

const char *ConsoleValueTypeName(ConsoleValueType type) {
  switch (type) {
    case ConsoleValueType::Bool: return "bool";
    case ConsoleValueType::Int: return "int";
    case ConsoleValueType::Float: return "float";
    case ConsoleValueType::String: return "string";
    default: return "unknown";
  }
}

bool LooksLikeConsoleHelp(const wchar_t *text) {
  if (!text || !SafeMem_IsReadable(text, sizeof(wchar_t))) return false;
  for (int i = 0; i < 512; ++i) {
    wchar_t character = 0;
    if (!SafeMem_Read(text + i, &character, sizeof(character))) return false;
    if (character == 0) return true;
  }
  return true;
}

std::string WideToUtf8(const wchar_t *text, int32_t length) {
  if (!text || length <= 0) return "";
  int written = WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
  if (written <= 0) return "";
  std::string out((size_t)written, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, length, &out[0], written, nullptr, nullptr);
  return out;
}

std::string SanitizeText(const std::string &text) {
  std::string out = text;
  for (char &character : out) {
    if (character == '\n' || character == '\r' || character == '\t') character = ' ';
    if (character == '|') character = '/';
  }
  return out;
}

struct ConsoleObjectRecord {
  std::string name;
  std::string value;
  std::string help;
  uint32_t flags = 0;
  ConsoleValueType type = ConsoleValueType::None;
  bool isVariable = false;
  bool hasValue = false;
};

bool NameLess(const ConsoleObjectRecord &left, const ConsoleObjectRecord &right) {
  const int comparison = _stricmp(left.name.c_str(), right.name.c_str());
  if (comparison != 0) return comparison < 0;
  return left.name < right.name;
}

}

ConsoleObjectSlots ResolveConsoleObjectSlots(const EngineState &state, void *consoleObject) {
  static ConsoleObjectSlots cached;
  static bool cachedValid = false;
  static int helpProbeAttempts = 0;
  if (cachedValid) return cached;

  ConsoleObjectSlots slots;
  if (!consoleObject || !SafeMem_IsReadable(consoleObject, sizeof(void *))) return slots;
  void **vtable = *(void ***)consoleObject;
  if (!vtable || !SafeMem_IsReadable(vtable, sizeof(void *) * 6)) return slots;

  const bool helpTextLayout = (state.UEMajor > 5) || (state.UEMajor == 5 && state.UEMinor >= 8);
  bool hasVirtualHelp = false;
  void *helpFunction = vtable[1];
  if (helpFunction && (uintptr_t)helpFunction > 0x10000 && (uintptr_t)helpFunction < 0x7FFFFFFFFFFFULL) {
    const wchar_t *helpText = nullptr;
    __try {
      helpText = reinterpret_cast<const wchar_t *(*)(void *)>(helpFunction)(consoleObject);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      helpText = nullptr;
    }
    hasVirtualHelp = LooksLikeConsoleHelp(helpText);
  }

  if (hasVirtualHelp) {
    slots.help = 1;
    slots.flags = helpTextLayout ? 4 : 3;
    slots.setFlags = helpTextLayout ? 5 : 4;
  } else if (helpTextLayout) {

    if (++helpProbeAttempts < 8) return slots;
    slots.flags = 1;
    slots.setFlags = 2;
  } else {
    slots.flags = 3;
    slots.setFlags = 4;
  }

  if (state.UEMajor > 0 && slots.setFlags >= 0) {
    int baseAfterSetFlags = 12;
    int asVariableAfterSetFlags = 1;
    int getBoolAfterBase = 1;
    int getIntAfterBase = 2;
    int getFloatAfterBase = 3;
    int getStringAfterBase = 4;
    bool hasBoolVariable = true;

    if (state.UEMajor == 4 && state.UEMinor == 23) {
      baseAfterSetFlags = 10;
      hasBoolVariable = false;
      getBoolAfterBase = -1;
      getIntAfterBase = 1;
      getFloatAfterBase = 2;
      getStringAfterBase = 3;
    } else if (state.UEMajor > 5 || (state.UEMajor == 5 && state.UEMinor >= 8)) {
      baseAfterSetFlags = 16;
      asVariableAfterSetFlags = 2;
      getBoolAfterBase = 3;
      getIntAfterBase = 4;
      getFloatAfterBase = 5;
      getStringAfterBase = 6;
    } else if (state.UEMajor == 5 && state.UEMinor == 7) {
      baseAfterSetFlags = 17;
      asVariableAfterSetFlags = 3;
      getBoolAfterBase = 3;
      getIntAfterBase = 4;
      getFloatAfterBase = 5;
      getStringAfterBase = 6;
    } else if (state.UEMajor == 5 && state.UEMinor == 6) {
      baseAfterSetFlags = 17;
      asVariableAfterSetFlags = 3;
      getBoolAfterBase = 2;
      getIntAfterBase = 3;
      getFloatAfterBase = 4;
      getStringAfterBase = 5;
    } else if (state.UEMajor == 5 && state.UEMinor == 5) {
      baseAfterSetFlags = 15;
      asVariableAfterSetFlags = 3;
      getBoolAfterBase = 2;
      getIntAfterBase = 3;
      getFloatAfterBase = 4;
      getStringAfterBase = 5;
    } else if (state.UEMajor == 5 && state.UEMinor == 4) {
      baseAfterSetFlags = 13;
      asVariableAfterSetFlags = 2;
      getBoolAfterBase = 2;
      getIntAfterBase = 3;
      getFloatAfterBase = 4;
      getStringAfterBase = 5;
    }

    slots.asVariable = slots.setFlags + asVariableAfterSetFlags;
    slots.isBool = hasBoolVariable ? slots.asVariable + 1 : -1;
    slots.isInt = slots.asVariable + (hasBoolVariable ? 2 : 1);
    slots.isFloat = slots.asVariable + (hasBoolVariable ? 3 : 2);

    const int variableBase = slots.setFlags + baseAfterSetFlags;
    slots.getBool = hasBoolVariable ? variableBase + getBoolAfterBase : -1;
    slots.getInt = variableBase + getIntAfterBase;
    slots.getFloat = variableBase + getFloatAfterBase;
    slots.getString = variableBase + getStringAfterBase;
  }

  cached = slots;
  cachedValid = true;
  return slots;
}

ConsoleValueResult ReadConsoleValue(void *consoleObject, const ConsoleObjectSlots &slots) {
  ConsoleValueResult result;
  if (!consoleObject || slots.asVariable < 0 || slots.getString < 0)
    return result;
  if (!SafeMem_IsReadable(consoleObject, sizeof(void *))) return result;

  void **vtable = *(void ***)consoleObject;
  if (!vtable || !SafeMem_IsReadable(vtable, sizeof(void *) * (size_t)(slots.asVariable + 1)))
    return result;

  typedef void *(*AsVariable_t)(void *);
  typedef bool (*BoolGetter_t)(const void *);
  typedef int32_t (*IntGetter_t)(const void *);
  typedef float (*FloatGetter_t)(const void *);

  typedef void (*StringGetter_t)(const void *self, void *out);

  __try {
    AsVariable_t asVariable = (AsVariable_t)vtable[slots.asVariable];
    if (!asVariable) return result;
    void *variable = asVariable(consoleObject);
    if (!variable) {
      result.kind = ConsoleValueKind::Command;
      return result;
    }
    if (variable != consoleObject) return result;

    int highestSlot = slots.getString;
    const int optionalSlots[] = {slots.isBool, slots.isInt, slots.isFloat,
                                 slots.getBool, slots.getInt, slots.getFloat};
    for (int slot : optionalSlots) {
      if (slot > highestSlot) highestSlot = slot;
    }
    if (!SafeMem_IsReadable(vtable, sizeof(void *) * (size_t)(highestSlot + 1))) return result;

    bool isBool = false;
    bool isInt = false;
    bool isFloat = false;
    if (slots.isBool >= 0 && vtable[slots.isBool])
      isBool = ((BoolGetter_t)vtable[slots.isBool])(consoleObject);
    if (!isBool && slots.isInt >= 0 && vtable[slots.isInt])
      isInt = ((BoolGetter_t)vtable[slots.isInt])(consoleObject);
    if (!isBool && !isInt && slots.isFloat >= 0 && vtable[slots.isFloat])
      isFloat = ((BoolGetter_t)vtable[slots.isFloat])(consoleObject);

    if (isBool && slots.getBool >= 0 && vtable[slots.getBool]) {
      const bool value = ((BoolGetter_t)vtable[slots.getBool])(consoleObject);
      snprintf(result.value, sizeof(result.value), "%s", value ? "true" : "false");
      result.kind = ConsoleValueKind::Value;
      result.type = ConsoleValueType::Bool;
      return result;
    }
    if (isInt && slots.getInt >= 0 && vtable[slots.getInt]) {
      snprintf(result.value, sizeof(result.value), "%d",
               ((IntGetter_t)vtable[slots.getInt])(consoleObject));
      result.kind = ConsoleValueKind::Value;
      result.type = ConsoleValueType::Int;
      return result;
    }
    if (isFloat && slots.getFloat >= 0 && vtable[slots.getFloat]) {
      snprintf(result.value, sizeof(result.value), "%g",
               ((FloatGetter_t)vtable[slots.getFloat])(consoleObject));
      result.kind = ConsoleValueKind::Value;
      result.type = ConsoleValueType::Float;
      return result;
    }

    StringGetter_t getString = (StringGetter_t)vtable[slots.getString];
    if (!getString) return result;

    struct EngineFString {
      wchar_t *Data;
      int32_t Num;
      int32_t Max;
    } text = {};
    getString(consoleObject, &text);

    if (text.Num < 0 || text.Num > (1 << 20)) return result;
    if (text.Num > 0 && text.Max < text.Num) return result;

    result.kind = ConsoleValueKind::Value;
    result.type = ConsoleValueType::String;
    if (text.Num == 0) return result;

    int32_t length = text.Num;
    if (length > (int32_t)sizeof(result.value) - 1) length = (int32_t)sizeof(result.value) - 1;
    if (!text.Data || !SafeMem_IsReadable(text.Data, sizeof(wchar_t) * (size_t)(length + 1)))
      return result;

    const int written = WideCharToMultiByte(CP_UTF8, 0, text.Data, length, result.value,
                                            (int)sizeof(result.value) - 1, nullptr, nullptr);
    if (written <= 0) {
      result.kind = ConsoleValueKind::Unavailable;
      return result;
    }
    result.value[written] = '\0';
    for (int i = 0; i < written; ++i) {
      if (result.value[i] == '\n' || result.value[i] == '\r' || result.value[i] == '\t')
        result.value[i] = ' ';
    }
    return result;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    result.kind = ConsoleValueKind::Faulted;
    result.value[0] = '\0';
    return result;
  }
}

namespace {

bool ReadConsoleHelp(void *consoleObject, const ConsoleObjectSlots &slots, char *out,
                     size_t outSize) {
  if (!consoleObject || slots.help < 0 || !out || outSize == 0) return false;

  __try {
    void **vtable = *(void ***)consoleObject;
    if (!vtable || !SafeMem_IsReadable(vtable, sizeof(void *) * (size_t)(slots.help + 1)))
      return false;
    typedef const wchar_t *(*GetHelp_t)(void *);
    GetHelp_t getHelp = (GetHelp_t)vtable[slots.help];
    if (!getHelp) return false;
    const wchar_t *text = getHelp(consoleObject);
    if (!text) return false;

    int32_t length = 0;
    while (length < 512 && text[length] != 0) ++length;
    if (length == 0) return false;
    const int written = WideCharToMultiByte(CP_UTF8, 0, text, length, out, (int)outSize - 1,
                                            nullptr, nullptr);
    if (written <= 0) return false;
    out[written] = '\0';
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool ReadConsoleFlags(void *consoleObject, const ConsoleObjectSlots &slots, uint32_t *outFlags) {
  if (!consoleObject || slots.flags < 0 || !outFlags) return false;

  __try {
    void **vtable = *(void ***)consoleObject;
    if (!vtable || !SafeMem_IsReadable(vtable, sizeof(void *) * (size_t)(slots.flags + 1)))
      return false;
    typedef uint32_t (*GetFlags_t)(const void *);
    GetFlags_t getFlags = (GetFlags_t)vtable[slots.flags];
    if (!getFlags) return false;
    *outFlags = getFlags(consoleObject);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

}

void DumpConsoleVariables(const EngineState &state, void *dataPtr, uint32_t *allocationFlags,
                          int32_t arrayNum, int32_t stride) {
  if (!RuntimeConfig::Load(nullptr).DumpAllDeveloperCommands) return;
  if (!dataPtr || !allocationFlags || arrayNum <= 0 || arrayNum > 200000) return;

  char consoleDir[512] = {0};
  GetConsoleDumpDirectory(state, consoleDir, sizeof(consoleDir));

  std::vector<ConsoleObjectRecord> records;
  records.reserve((size_t)arrayNum);

  ConsoleObjectSlots slots;
  int faultCount = 0;
  int unreadableCount = 0;
  std::string firstFaultName;

  for (int32_t i = 0; i < arrayNum; ++i) {
    const int32_t dwordIndex = i / 32;
    const uint32_t bitMask = 1u << (i % 32);
    if (dwordIndex > (200000 / 32)) break;
    if ((allocationFlags[dwordIndex] & bitMask) == 0) continue;

    const uint8_t *element = (const uint8_t *)dataPtr + ((size_t)i * (size_t)stride);

    void *nameData = nullptr;
    int32_t nameLength = 0;
    if (!SafeMem_Read(element, &nameData, sizeof(void *))) continue;
    if (!SafeMem_Read(element + 8, &nameLength, sizeof(int32_t))) continue;
    if (!nameData || nameLength <= 0 || nameLength > 255) continue;

    const wchar_t *nameText = (const wchar_t *)nameData;
    const std::string name = WideToUtf8(nameText, nameLength);
    if (name.empty()) continue;

    void *consoleObject = nullptr;
    SafeMem_Read(element + 0x10, &consoleObject, sizeof(void *));
    if (!consoleObject) continue;

    if (slots.asVariable < 0) slots = ResolveConsoleObjectSlots(state, consoleObject);

    ConsoleObjectRecord record;
    record.name = name;

    const ConsoleValueResult value = ReadConsoleValue(consoleObject, slots);
    if (value.kind == ConsoleValueKind::Faulted) {
      if (firstFaultName.empty()) firstFaultName = name;
      ++faultCount;
      if (faultCount > 8) break;
      continue;
    }

    record.isVariable = value.kind == ConsoleValueKind::Value;
    record.hasValue = record.isVariable;
    record.type = value.type;
    if (record.isVariable) record.value = value.value;

    if (record.isVariable) {
      uint32_t flags = 0;
      if (ReadConsoleFlags(consoleObject, slots, &flags)) record.flags = flags;
    }

    char help[512] = {0};
    if (ReadConsoleHelp(consoleObject, slots, help, sizeof(help))) {
      record.help = SanitizeText(help);
    }

    if (!record.isVariable && value.kind == ConsoleValueKind::Unavailable) ++unreadableCount;

    records.push_back(record);
  }

  std::sort(records.begin(), records.end(), NameLess);

  int variableCount = 0;
  int commandCount = 0;
  for (const ConsoleObjectRecord &record : records) {
    if (record.isVariable) ++variableCount;
    else ++commandCount;
  }

  char valuesPath[512] = {0};
  snprintf(valuesPath, sizeof(valuesPath), "%sCVARs.ini", consoleDir);
  char referencePath[512] = {0};
  snprintf(referencePath, sizeof(referencePath), "%sCVarReference.txt", consoleDir);

  FILE *valuesFile = fopen(valuesPath, "w");
  if (valuesFile) {
    fprintf(valuesFile, "; Avalonium - CVar Values\n");
    fprintf(valuesFile, "; Game: %s\n", state.GameName);
    if (state.UEMajor > 0) fprintf(valuesFile, "; Engine: %d.%d\n", state.UEMajor, state.UEMinor);
    fprintf(valuesFile, "; %d console variables, %d console commands\n", variableCount,
            commandCount);
    fprintf(valuesFile,
            ";\n"
            "; Sorted by name. A name=value line holds the current value of a console\n"
            "; variable. A console command runs code instead of holding a value, so the\n"
            "; engine registers it as a command, not a variable: each entry in the\n"
            "; command block is a comment line that also carries its help text. Types,\n"
            "; flags and the help text of every entry are in CVarReference.txt.\n\n");

    for (const ConsoleObjectRecord &record : records) {
      if (!record.isVariable) continue;
      fprintf(valuesFile, "%s=%s\n", record.name.c_str(), record.value.c_str());
    }

    fprintf(valuesFile, "\n; --- console commands (%d, no value) ---\n", commandCount);
    for (const ConsoleObjectRecord &record : records) {
      if (record.isVariable) continue;
      if (record.help.empty()) {
        fprintf(valuesFile, "; %s\n", record.name.c_str());
      } else {
        fprintf(valuesFile, "; %s : %s\n", record.name.c_str(), record.help.c_str());
      }
    }

    fclose(valuesFile);

    char logBuf[512] = {0};
    snprintf(logBuf, sizeof(logBuf),
             "[ConsoleEnabler] Dumped %d CVAR values (%d commands) to %s", variableCount,
             commandCount, valuesPath);
    EngineDiscovery::WriteLogLine(logBuf);
  }

  FILE *referenceFile = fopen(referencePath, "w");
  if (referenceFile) {
    fprintf(referenceFile, "=== Avalonium - Console Reference ===\n");
    fprintf(referenceFile, "Game: %s\n", state.GameName);
    if (state.UEMajor > 0) fprintf(referenceFile, "Engine: %d.%d\n", state.UEMajor, state.UEMinor);
    fprintf(referenceFile, "Console variables: %d\n", variableCount);
    fprintf(referenceFile, "Console commands: %d\n", commandCount);
    fprintf(referenceFile,
            "\n"
            "Format: name | type | flags | value | help\n"
            "Types: bool, int, float, string. Flags are the engine's console variable\n"
            "flags plus the source that set the value (SetBy). A string value is\n"
            "quoted so an empty one is visible.\n"
            "\n"
            "=== VARIABLES ===\n");

    for (const ConsoleObjectRecord &record : records) {
      if (!record.isVariable) continue;
      const std::string flags = ConsoleFlagNames(record.flags);
      if (record.type == ConsoleValueType::String) {
        fprintf(referenceFile, "%s | %s | %s | \"%s\" | %s\n", record.name.c_str(),
                ConsoleValueTypeName(record.type), flags.empty() ? "-" : flags.c_str(),
                record.value.c_str(), record.help.c_str());
      } else {
        fprintf(referenceFile, "%s | %s | %s | %s | %s\n", record.name.c_str(),
                ConsoleValueTypeName(record.type), flags.empty() ? "-" : flags.c_str(),
                record.value.c_str(), record.help.c_str());
      }
    }

    fprintf(referenceFile, "\n=== COMMANDS ===\n");
    for (const ConsoleObjectRecord &record : records) {
      if (record.isVariable) continue;
      fprintf(referenceFile, "%s | %s\n", record.name.c_str(), record.help.c_str());
    }

    fclose(referenceFile);

    char logBuf[512] = {0};
    snprintf(logBuf, sizeof(logBuf),
             "[ConsoleEnabler] Wrote console reference (%d variables, %d commands) to %s",
             variableCount, commandCount, referencePath);
    EngineDiscovery::WriteLogLine(logBuf);
  }

  if (faultCount > 0) {
    char logBuf[512] = {0};
    snprintf(logBuf, sizeof(logBuf),
             "[ConsoleEnabler] CVAR value reading hit %d fault(s), first on %s; %d entries unreadable.",
             faultCount, firstFaultName.c_str(), unreadableCount);
    EngineDiscovery::WriteLogLine(logBuf);
  }
}

}
