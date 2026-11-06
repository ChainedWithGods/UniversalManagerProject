#include "ConsoleInternal.h"

void GetConsoleDumpDirectory(const EngineState &state, char* outBuffer, size_t bufferSize) {
  if (!outBuffer || bufferSize == 0) return;

  std::string logPath = EngineDiscovery::GetLogPath();
  char logDir[512] = {0};
  strncpy(logDir, logPath.c_str(), sizeof(logDir) - 1);

  char *sl = strrchr(logDir, '\\');
  if (!sl) sl = strrchr(logDir, '/');
  if (sl) *(sl + 1) = '\0';

  char safeGameName[256] = {0};
  strncpy(safeGameName, state.GameName, sizeof(safeGameName) - 1);
  for (char *p = safeGameName; *p; ++p) {
    if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' || *p == '?' ||
        *p == '"' || *p == '<' || *p == '>' || *p == '|') {
      *p = '_';
    }
  }

  snprintf(outBuffer, bufferSize, "%s%s\\Console\\", logDir, safeGameName);
  outBuffer[bufferSize - 1] = '\0';

  std::error_code ec;
  std::filesystem::create_directories(outBuffer, ec);
}

namespace ConsoleEnabler {

UObject *TrySEHConstruct(const EngineState &state, UClass *cls, UObject *outer, FName name, uint32_t setFlags) noexcept {
  if (!state.StaticConstructObject || !cls) return nullptr;

  UObject *result = nullptr;
  __try {
    if (state.bStaticConstructTakesParams) {
      uint8_t params[256] = {0};
      *(UClass **)(&params[0]) = cls;
      *(UObject **)(&params[8]) = outer;
      *(uint32_t *)(&params[16]) = name.ComparisonIndex;
      *(uint32_t *)(&params[20]) = name.Number;
      *(uint32_t *)(&params[24]) = setFlags;

      typedef UObject* (__fastcall *ParamsFn_t)(const void* Params);
      ParamsFn_t fn = reinterpret_cast<ParamsFn_t>(state.StaticConstructObject);
      result = fn(params);
    } else {
      typedef UObject* (__fastcall *DirectFn_t)(
          const UClass* Class,
          UObject* InOuter,
          FName Name,
          uint32_t SetFlags,
          uint32_t InternalSetFlags,
          UObject* Template,
          bool bCopyTransientsFromClassDefaults,
          void* InstanceGraph,
          bool bAssumeTemplateIsArchetype,
          void* ExternalPackage);
      DirectFn_t fn = reinterpret_cast<DirectFn_t>(state.StaticConstructObject);
      result = fn(cls, outer, name, setFlags, 0, nullptr, false, nullptr, false, nullptr);
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    result = nullptr;
  }
  return result;
}

UObject *TrySEHConstruct(StaticConstructObject_Internal_t fn, void *params) noexcept {
  if (!fn || !params) return nullptr;
  UObject *result = nullptr;
  __try {
    result = fn(params);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    result = nullptr;
  }
  return result;
}

bool StartsWith(const std::string &text, const char *prefix) {
  const size_t prefixLen = strlen(prefix);
  return text.size() >= prefixLen && text.compare(0, prefixLen, prefix) == 0;
}

bool EqualsInsensitiveAscii(const std::string &text, const char *rhs) {
  if (!rhs) return false;
  const size_t rhsLen = strlen(rhs);
  if (text.size() != rhsLen) return false;

  for (size_t i = 0; i < rhsLen; ++i) {
    char a = text[i];
    char b = rhs[i];
    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
    if (a != b) return false;
  }
  return true;
}

bool ContainsInsensitiveAscii(const std::string &text, const char *needle) {
  if (!needle || !*needle) return false;

  const size_t needleLen = strlen(needle);
  if (needleLen > text.size()) return false;

  for (size_t start = 0; start + needleLen <= text.size(); ++start) {
    bool matches = true;
    for (size_t i = 0; i < needleLen; ++i) {
      char a = text[start + i];
      char b = needle[i];
      if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
      if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
      if (a != b) {
        matches = false;
        break;
      }
    }
    if (matches) return true;
  }

  return false;
}

bool IsDefaultObjectName(const std::string &name) {
  return StartsWith(name, "Default__");
}

bool IsClassObject(const EngineState &state, UObject *object) {
  if (!object || !state.UClassClass) return false;
  if (object == (UObject*)state.UClassClass) return true;
  UClass *cls = EngineDiscovery::GetObjectClass(state, object);
  if (!cls) return false;
  if (cls == state.UClassClass) return true;
  return EngineDiscovery::IsStructA(state, (UStruct*)cls, (UStruct*)state.UClassClass);
}

bool IsObjectA(const EngineState &state, UObject *object, UClass *baseClass) {
  if (!object || !baseClass) return false;

  UClass *cls = EngineDiscovery::GetObjectClass(state, object);
  if (!cls) return false;
  if (cls == baseClass) return true;

  return EngineDiscovery::IsStructA(state, (UStruct *)cls, (UStruct *)baseClass);
}

UObject *FindOwningClassObject(const EngineState &state, UObject *object) {
  UObject *outer = EngineDiscovery::GetObjectOuter(state, object);
  for (int depth = 0; depth < 16 && outer; ++depth) {
    if (IsClassObject(state, outer)) {
      return outer;
    }
    outer = EngineDiscovery::GetObjectOuter(state, outer);
  }
  return nullptr;
}

UClass *FindClassByNameExact(const EngineState &state, const char *className) {
  UClass **cachedPtr = nullptr;
  if (strcmp(className, "Console") == 0)
    cachedPtr = &g_CachedConsoleClass;
  else if (strcmp(className, "GameEngine") == 0)
    cachedPtr = &g_CachedGameEngineClass;
  else if (strcmp(className, "Engine") == 0)
    cachedPtr = &g_CachedEngineClass;
  else if (strcmp(className, "GameViewportClient") == 0)
    cachedPtr = &g_CachedGameViewportClientClass;
  else if (strcmp(className, "LocalPlayer") == 0)
    cachedPtr = &g_CachedLocalPlayerClass;
  else if (strcmp(className, "GameplayStatics") == 0)
    cachedPtr = &g_CachedGameplayStaticsClass;

  if (cachedPtr && *cachedPtr) {
    return *cachedPtr;
  }

  static std::unordered_map<std::string, UClass*> s_DynamicClassCache;
  auto it = s_DynamicClassCache.find(className);
  if (it != s_DynamicClassCache.end()) {
      if (cachedPtr) *cachedPtr = it->second;
      return it->second;
  }

  std::vector<UObject *> fastMatches = EngineDiscovery::FindObjectsByNameFast(
      const_cast<EngineState &>(state), className);

  for (UObject *fast : fastMatches) {
    if (IsClassObject(state, fast)) {
      UClass *result = (UClass *)fast;
      if (cachedPtr) *cachedPtr = result;
      s_DynamicClassCache[className] = result;
      return result;
    }
  }

  if (state.bIndexBuilt) {
    s_DynamicClassCache[className] = nullptr;
    return nullptr;
  }

  const int32_t count = EngineDiscovery::GetObjectCount(state);
  for (int32_t i = 0; i < count; ++i) {
    UObject *object = EngineDiscovery::GetObjectByIndex(state, i);
    if (!IsClassObject(state, object)) continue;
    if (EngineDiscovery::GetObjectNameStr(state, object) == className) {
      UClass *result = (UClass *)object;
      if (cachedPtr) *cachedPtr = result;
      s_DynamicClassCache[className] = result;
      return result;
    }
  }

  s_DynamicClassCache[className] = nullptr;
  return nullptr;
}

UObject *FindObjectByNameExact(const EngineState &state, const char *objectName) {
  std::vector<UObject *> fastMatches = EngineDiscovery::FindObjectsByNameFast(
      const_cast<EngineState &>(state), objectName);

  for (UObject *fast : fastMatches) {
    if (fast) {
      return fast;
    }
  }

  if (state.bIndexBuilt) {
    return nullptr;
  }

  const int32_t count = EngineDiscovery::GetObjectCount(state);
  for (int32_t i = 0; i < count; ++i) {
    UObject *object = EngineDiscovery::GetObjectByIndex(state, i);
    if (!object) continue;
    if (EngineDiscovery::GetObjectNameStr(state, object) == objectName) {
      return object;
    }
  }

  return nullptr;
}

UObject *FindFirstObjectOfClass(const EngineState &state, UClass *baseClass, bool wantDefaultObject) {
  if (!baseClass) return nullptr;

  if (state.bIndexBuilt) {
    auto selectInstance = [&](const std::vector<UObject *> &instances) -> UObject * {
      UObject *firstMatch = nullptr;
      for (UObject *object : instances) {
        if (!object) continue;

        const std::string name = EngineDiscovery::GetObjectNameStr(state, object);
        const bool isDefault = IsDefaultObjectName(name);
        if (wantDefaultObject != isDefault) continue;

        if (!firstMatch) {
          firstMatch = object;
        }

        if (!wantDefaultObject) {
          const std::string fullName = EngineDiscovery::BuildFullName(state, object);
          if (fullName.find("Transient.") != std::string::npos ||
              fullName.find("Engine.") != std::string::npos) {
            return object;
          }
        }
      }
      return firstMatch;
    };

    std::vector<UObject *> exactInstances;
    {
      std::lock_guard<std::recursive_mutex> lock(*state.IndexMutex);
      const auto it = state.ClassToInstances.find(baseClass);
      if (it != state.ClassToInstances.end()) {
        exactInstances = it->second;
      }
    }
    if (UObject *exactMatch = selectInstance(exactInstances)) {
      return exactMatch;
    }

    std::vector<UObject *> instances = EngineDiscovery::FindObjectsByClassFast(
        const_cast<EngineState &>(state), baseClass);
    if (UObject *derivedMatch = selectInstance(instances)) {
      return derivedMatch;
    }

    return nullptr;
  }

  const int32_t count = EngineDiscovery::GetObjectCount(state);
  for (int32_t i = 0; i < count; ++i) {
    UObject *object = EngineDiscovery::GetObjectByIndex(state, i);
    if (!object || !IsObjectA(state, object, baseClass)) continue;

    const std::string name = EngineDiscovery::GetObjectNameStr(state, object);
    const bool isDefault = IsDefaultObjectName(name);
    if (wantDefaultObject == isDefault) {
      return object;
    }
  }

  return nullptr;
}

UObject *FindExistingConsoleObject(const EngineState &state, UClass *consoleClass,
                                   UObject *expectedOuter) {
  if (!consoleClass || !expectedOuter) return nullptr;

  if (state.bIndexBuilt) {
    std::vector<UObject *> instances = EngineDiscovery::FindObjectsByClassFast(
        const_cast<EngineState &>(state), consoleClass);
    for (UObject *object : instances) {
      if (!object) continue;
      const std::string name = EngineDiscovery::GetObjectNameStr(state, object);

      if (IsDefaultObjectName(name)) continue;
      if (EngineDiscovery::GetObjectOuter(state, object) != expectedOuter) continue;
      return object;
    }
    return nullptr;
  }

  const int32_t count = EngineDiscovery::GetObjectCount(state);

  for (int32_t i = 0; i < count; ++i) {
    UObject *object = EngineDiscovery::GetObjectByIndex(state, i);
    if (!IsObjectA(state, object, consoleClass)) continue;

    const std::string name = EngineDiscovery::GetObjectNameStr(state, object);
    if (IsDefaultObjectName(name)) continue;
    if (EngineDiscovery::GetObjectOuter(state, object) != expectedOuter) continue;
    return object;
  }

  return nullptr;
}

bool IsUsableConsoleObject(const EngineState &state, UObject *consoleObject, UClass *consoleClass) {
  if (!consoleObject || !consoleClass) return false;
  return IsObjectA(state, consoleObject, consoleClass);
}

void *ReadPtrAt(void *base, int32_t offset) {
  if (!base || offset < 0) return nullptr;
  return SafeMem_ReadPtr((uint8_t *)base + offset);
}

void *GetPropertyOwner(const EngineState &state, void *prop, bool useFPropertyLayout) {
  if (!prop) return nullptr;
  if (useFPropertyLayout) {
    uintptr_t ownerValue = 0;
    if (!SafeMem_Read((uint8_t *)prop + 0x10, &ownerValue, sizeof(ownerValue))) {
      return nullptr;
    }
    if (state.bUseMaskForFieldOwner) {
      ownerValue &= ~static_cast<uintptr_t>(1);
    }
    return reinterpret_cast<void *>(ownerValue);
  }
  return ReadPtrAt(prop, state.UObject_OuterOff);
}

std::string GetUPropertyTypeName(const EngineState &state, void *prop) {
  if (!prop) return "";
  UClass *propClass = EngineDiscovery::GetObjectClass(state, (UObject *)prop);
  return EngineDiscovery::GetObjectNameStr(state, (UObject *)propClass);
}

void *GuessArrayInnerProperty(const EngineState &state, void *prop, bool useFPropertyLayout) {
  if (!prop || !useFPropertyLayout) return nullptr;

  uint8_t propBuf[0xA0] = {};
  if (!SafeMem_Read(prop, propBuf, sizeof(propBuf))) {
    return nullptr;
  }

  int32_t propBaseEnd = state.FProperty_PropertyFlagsOff >= 0
                            ? (state.FProperty_PropertyFlagsOff + 8)
                            : (state.FProperty_ArrayDimOff >= 0
                                   ? (state.FProperty_ArrayDimOff + 16)
                                   : (state.FProperty_OffsetOff >= 0
                                          ? (state.FProperty_OffsetOff + 4)
                                          : 0x50));
  if (propBaseEnd < 0x40) {
    propBaseEnd = 0x40;
  }

  for (int32_t off = propBaseEnd; off <= 0x90; off += 8) {
    void *inner = *(void **)(propBuf + off);
    if (!inner) continue;

    const uintptr_t addr = reinterpret_cast<uintptr_t>(inner);
    if (addr < 0x10000 || addr >= 0x7FF000000000ULL) continue;

    const std::string innerType = EngineDiscovery::GetFFieldTypeName(state, inner);
    if (innerType.find("Property") == std::string::npos) continue;

    if (GetPropertyOwner(state, inner, true) == prop) {
      return inner;
    }
  }

  return nullptr;
}

PropertyLookupResult FindPropertyWithLayout(const EngineState &state, UStruct *cls, const std::string &propName, bool useFPropertyLayout) {
  PropertyLookupResult result = {};
  UStruct *current = cls;
  while (current) {
    void *prop = nullptr;
    if (useFPropertyLayout) {
      if (state.UStruct_ChildPropsOff < 0 || state.FProperty_OffsetOff < 0) {
        current = EngineDiscovery::GetSuperStruct(state, current);
        continue;
      }

      prop = EngineDiscovery::GetChildPropsHead(state, current);
      while (prop) {
        std::string fname = EngineDiscovery::GetFFieldName(state, prop);
        if (_stricmp(fname.c_str(), propName.c_str()) == 0) {
          result.Property = prop;
          result.UsedFPropertyLayout = true;
          result.TypeName = EngineDiscovery::GetFFieldTypeName(state, prop);
          SafeMem_Read((uint8_t *)prop + state.FProperty_OffsetOff, &result.Offset, 4);

          if (result.TypeName == "ArrayProperty") {
            void *inner = state.FArrayProperty_InnerOff >= 0
                              ? ReadPtrAt(prop, state.FArrayProperty_InnerOff)
                              : GuessArrayInnerProperty(state, prop, true);
            if (inner) {
              result.InnerTypeName = EngineDiscovery::GetFFieldTypeName(state, inner);
              if (result.InnerTypeName == "StructProperty" &&
                  state.FStructProperty_StructOff >= 0) {
                UObject *structObject = (UObject *)ReadPtrAt(inner, state.FStructProperty_StructOff);
                if (structObject) {
                  result.StructTypeName = EngineDiscovery::GetObjectNameStr(state, structObject);
                }
              }
            }
          }

          return result;
        }
        prop = EngineDiscovery::GetFFieldNext(state, prop);
      }
    } else {
      if (state.UStruct_ChildrenOff < 0 || state.UProperty_OffsetOff < 0) {
        current = EngineDiscovery::GetSuperStruct(state, current);
        continue;
      }

      prop = EngineDiscovery::GetChildrenHead(state, current);
      while (prop) {
        std::string fname = EngineDiscovery::GetFieldName(state, prop);
        if (_stricmp(fname.c_str(), propName.c_str()) == 0) {
          result.Property = prop;
          result.UsedFPropertyLayout = false;
          result.TypeName = GetUPropertyTypeName(state, prop);
          SafeMem_Read((uint8_t *)prop + state.UProperty_OffsetOff, &result.Offset, 4);
          return result;
        }
        prop = EngineDiscovery::GetFieldNext(state, prop);
      }
    }
    current = EngineDiscovery::GetSuperStruct(state, current);
  }

  return result;
}

int32_t FindPropertyOffset(const EngineState &state, UStruct *cls, const std::string &propName) {
  if (!cls) return -1;

  static std::unordered_map<std::string, int32_t> s_PropOffsetCache;
  std::string cacheKey = EngineDiscovery::GetObjectNameStr(state, (UObject *)cls) + "::" + propName;
  auto it = s_PropOffsetCache.find(cacheKey);
  if (it != s_PropOffsetCache.end()) {
      return it->second;
  }

  const bool preferFProperty = state.bUseFProperty && state.UStruct_ChildPropsOff >= 0;
  const PropertyLookupResult preferred = FindPropertyWithLayout(state, cls, propName, preferFProperty);
  if (preferred.Offset >= 0) {
    s_PropOffsetCache[cacheKey] = preferred.Offset;
    return preferred.Offset;
  }

  int32_t fallbackOffset = FindPropertyWithLayout(state, cls, propName, !preferFProperty).Offset;
  if (fallbackOffset >= 0) {
    s_PropOffsetCache[cacheKey] = fallbackOffset;
  }
  return fallbackOffset;
}

PropertyLookupResult FindPropertyInfo(const EngineState &state, UStruct *cls, const std::string &propName) {
  if (!cls) return {};

  const bool preferFProperty = state.bUseFProperty && state.UStruct_ChildPropsOff >= 0;
  PropertyLookupResult preferred = FindPropertyWithLayout(state, cls, propName, preferFProperty);
  if (preferred.Offset >= 0) {
    return preferred;
  }

  return FindPropertyWithLayout(state, cls, propName, !preferFProperty);
}

void LogPropertyPreview(const EngineState &state, UStruct *cls, const char *header, bool useFPropertyLayout) {
  if (!cls || !header) return;

  EngineDiscovery::WriteLogLine(header);

  UStruct *current = cls;
  int32_t logged = 0;
  while (current && logged < 12) {
    char classLine[256] = {};
    const std::string className = EngineDiscovery::GetObjectNameStr(state, (UObject *)current);
    snprintf(classLine, sizeof(classLine),
             "[ConsoleEnabler]   class=%s path=%s",
             className.empty() ? "<unnamed>" : className.c_str(),
             useFPropertyLayout ? "ChildProperties" : "Children");
    EngineDiscovery::WriteLogLine(classLine);

    void *prop = useFPropertyLayout
                     ? EngineDiscovery::GetChildPropsHead(state, current)
                     : EngineDiscovery::GetChildrenHead(state, current);

    int32_t classLogged = 0;
    while (prop && logged < 12 && classLogged < 6) {
      const std::string propName =
          useFPropertyLayout ? EngineDiscovery::GetFFieldName(state, prop)
                             : EngineDiscovery::GetFieldName(state, prop);
      if (!propName.empty()) {
        char propLine[256] = {};
        snprintf(propLine, sizeof(propLine), "[ConsoleEnabler]     %s", propName.c_str());
        EngineDiscovery::WriteLogLine(propLine);
        ++logged;
        ++classLogged;
      }

      prop = useFPropertyLayout ? EngineDiscovery::GetFFieldNext(state, prop)
                                : EngineDiscovery::GetFieldNext(state, prop);
    }

    current = EngineDiscovery::GetSuperStruct(state, current);
  }
}

void LogFunctionPreview(const EngineState &state, UStruct *cls, const char *header) {
  if (!cls || !header || state.UStruct_ChildrenOff < 0) return;

  EngineDiscovery::WriteLogLine(header);

  UStruct *current = cls;
  int32_t logged = 0;
  while (current && logged < 16) {
    char classLine[256] = {};
    const std::string className = EngineDiscovery::GetObjectNameStr(state, (UObject *)current);
    snprintf(classLine, sizeof(classLine),
             "[ConsoleEnabler]   class=%s functions",
             className.empty() ? "<unnamed>" : className.c_str());
    EngineDiscovery::WriteLogLine(classLine);

    void *child = EngineDiscovery::GetChildrenHead(state, current);
    int32_t classLogged = 0;
    while (child && logged < 16 && classLogged < 8) {
      const std::string name = EngineDiscovery::GetObjectNameStr(state, (UObject *)child);
      if (!name.empty()) {
        char line[256] = {};
        snprintf(line, sizeof(line), "[ConsoleEnabler]     %s", name.c_str());
        EngineDiscovery::WriteLogLine(line);
        ++logged;
        ++classLogged;
      }
      child = EngineDiscovery::GetFieldNext(state, child);
    }

    current = EngineDiscovery::GetSuperStruct(state, current);
  }
}

void LogFunctionKeywordMatches(const EngineState &state, UStruct *cls, const char *header) {
  if (!cls || !header || state.UStruct_ChildrenOff < 0) return;

  EngineDiscovery::WriteLogLine(header);

  UStruct *current = cls;
  int32_t logged = 0;
  while (current && logged < 12) {
    void *child = EngineDiscovery::GetChildrenHead(state, current);
    while (child && logged < 12) {
      const std::string name = EngineDiscovery::GetObjectNameStr(state, (UObject *)child);
      if (ContainsInsensitiveAscii(name, "state") ||
          ContainsInsensitiveAscii(name, "input") ||
          ContainsInsensitiveAscii(name, "console") ||
          ContainsInsensitiveAscii(name, "open") ||
          ContainsInsensitiveAscii(name, "typing")) {
        char line[256] = {};
        snprintf(line, sizeof(line), "[ConsoleEnabler]     %s", name.c_str());
        EngineDiscovery::WriteLogLine(line);
        ++logged;
      }
      child = EngineDiscovery::GetFieldNext(state, child);
    }
    current = EngineDiscovery::GetSuperStruct(state, current);
  }

  if (logged == 0) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler]     <no matching UConsole functions found>");
  }
}

void LogConsoleClassDiagnostics(const EngineState &state, UObject *consoleObject, const char *reason, bool includeFunctionDetails) {
  if (!consoleObject || !reason) return;

  UClass *consoleClass = EngineDiscovery::GetObjectClass(state, consoleObject);
  if (!consoleClass) return;

  char header[512] = {};
  snprintf(header, sizeof(header), "[ConsoleEnabler] %s on %s", reason,
           EngineDiscovery::BuildFullName(state, (UObject *)consoleClass).c_str());
  EngineDiscovery::WriteLogLine(header);

  if (state.UStruct_ChildPropsOff >= 0) {
    LogPropertyPreview(state, (UStruct *)consoleClass, "[ConsoleEnabler] UConsole property preview via ChildProperties:", true);
  }
  if (state.UStruct_ChildrenOff >= 0) {
    LogPropertyPreview(state, (UStruct *)consoleClass, "[ConsoleEnabler] UConsole property preview via Children:", false);
  }

  if (includeFunctionDetails && state.UStruct_ChildrenOff >= 0) {
    LogFunctionKeywordMatches(state, (UStruct *)consoleClass, "[ConsoleEnabler] UConsole keyword-matched functions:");
    LogFunctionPreview(state, (UStruct *)consoleClass, "[ConsoleEnabler] UConsole function preview:");
  }
}

bool EnsureFakeGotoStateResolved(const EngineState &state, UObject *consoleObject) {
  if (!consoleObject) return false;

  UClass *consoleClass = EngineDiscovery::GetObjectClass(state, consoleObject);
  if (!consoleClass) return false;

  if (!g_TriedResolveFakeGotoStateFunction) {
    g_TriedResolveFakeGotoStateFunction = true;
    g_FakeGotoStateFunction = FindFunctionByNameLoose(state, (UStruct *)consoleClass, "FakeGotoState");

  }

  return g_FakeGotoStateFunction != nullptr;
}

void *FindFunctionByNameLoose(const EngineState &state, UStruct *cls, const char *funcName) {
  if (!cls || !funcName || !*funcName || state.UStruct_ChildrenOff < 0) return nullptr;

  void *exact = EngineDiscovery::FindFunctionByName(state, cls, funcName);
  if (exact) {
    return exact;
  }

  UStruct *current = cls;
  while (current) {
    void *child = EngineDiscovery::GetChildrenHead(state, current);
    int32_t count = 0;
    while (child && count < 500) {
      const std::string name = EngineDiscovery::GetObjectNameStr(state, (UObject *)child);
      if (EqualsInsensitiveAscii(name, funcName)) {
        return child;
      }
      child = EngineDiscovery::GetFieldNext(state, child);
      ++count;
    }
    current = EngineDiscovery::GetSuperStruct(state, current);
  }

  return nullptr;
}

int32_t FindViewportConsoleOffset(const EngineState &state, UObject *gameViewport) {
  if (!gameViewport) return -1;

  UClass *viewportClass = EngineDiscovery::GetObjectClass(state, gameViewport);
  if (!viewportClass) return -1;

  int32_t viewportConsoleOffset = FindPropertyOffset(state, (UStruct *)viewportClass, "ViewportConsole");
  if (viewportConsoleOffset >= 0) {
    return viewportConsoleOffset;
  }

  UClass *baseViewportClass = FindClassByNameExact(state, "GameViewportClient");
  if (baseViewportClass && baseViewportClass != viewportClass) {
    viewportConsoleOffset = FindPropertyOffset(state, (UStruct *)baseViewportClass, "ViewportConsole");
    if (viewportConsoleOffset >= 0) {
      char buffer[256] = {};
      snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] Resolved ViewportConsole via base class GameViewportClient at offset 0x%X", viewportConsoleOffset);
      EngineDiscovery::WriteLogLine(buffer);
      return viewportConsoleOffset;
    }
  }

  if (!g_LoggedViewportConsoleFailureDetails) {
    g_LoggedViewportConsoleFailureDetails = true;

    char header[512] = {};
    snprintf(header, sizeof(header), "[ConsoleEnabler] Failed to find ViewportConsole on viewport class %s",
             EngineDiscovery::BuildFullName(state, (UObject *)viewportClass).c_str());
    EngineDiscovery::WriteLogLine(header);

    if (state.UStruct_ChildPropsOff >= 0) {
      LogPropertyPreview(state, (UStruct *)viewportClass, "[ConsoleEnabler] Viewport property preview via ChildProperties:", true);
    }
    if (state.UStruct_ChildrenOff >= 0) {
      LogPropertyPreview(state, (UStruct *)viewportClass, "[ConsoleEnabler] Viewport property preview via Children:", false);
    }
  }

  return -1;
}

int32_t ResolveCachedFNameIndex(const EngineState &state, const char *name) {
  if (!name || !*name) return -1;
  return EngineDiscovery::ResolveFNameIndex(const_cast<EngineState &>(state), name);
}

UObject *FindFirstLocalPlayer(const EngineState &state) {

  if (state.GEngine && SafeMem_IsReadable(state.GEngine, 32)) {
    UObject* engine = state.GEngine;
    UClass* engineClass = EngineDiscovery::GetObjectClass(state, engine);
    if (engineClass) {
      int32_t vpOff = FindPropertyOffset(state, (UStruct*)engineClass, "GameViewport");
      if (vpOff >= 0) {
        UObject* viewport = (UObject*)SafeMem_ReadPtr((uint8_t*)engine + vpOff);
        if (viewport && SafeMem_IsReadable(viewport, 32)) {
          UClass* vpClass = EngineDiscovery::GetObjectClass(state, viewport);
          if (vpClass) {
            int32_t giOff = FindPropertyOffset(state, (UStruct*)vpClass, "GameInstance");
            if (giOff >= 0) {
              UObject* gi = (UObject*)SafeMem_ReadPtr((uint8_t*)viewport + giOff);
              if (gi && SafeMem_IsReadable(gi, 32)) {
                UClass* giClass = EngineDiscovery::GetObjectClass(state, gi);
                if (giClass) {
                  int32_t lpOff = FindPropertyOffset(state, (UStruct*)giClass, "LocalPlayers");
                  if (lpOff >= 0) {
                    uint8_t tarrayBuf[16] = {};
                    if (SafeMem_Read((uint8_t*)gi + lpOff, tarrayBuf, 16)) {
                      void* data = *(void**)tarrayBuf;
                      int32_t count = *(int32_t*)(tarrayBuf + 8);
                      if (data && count > 0) {
                        UObject* lp = (UObject*)SafeMem_ReadPtr((uint8_t*)data);
                        if (lp && SafeMem_IsReadable(lp, 32)) {
                          return lp;
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  UClass *localPlayerClass = FindClassByNameExact(state, "LocalPlayer");
  if (!localPlayerClass) return nullptr;
  return FindFirstObjectOfClass(state, localPlayerClass, false);
}

UObject *FindPlayerController(const EngineState &state) {
  UObject *localPlayer = FindFirstLocalPlayer(state);
  if (!localPlayer) return nullptr;

  UClass *localPlayerClass = EngineDiscovery::GetObjectClass(state, localPlayer);
  if (!localPlayerClass) return nullptr;

  const int32_t pcOffset = FindPropertyOffset(state, (UStruct *)localPlayerClass, "PlayerController");
  if (pcOffset < 0) return nullptr;

  UObject* pc = (UObject *)SafeMem_ReadPtr((uint8_t *)localPlayer + pcOffset);
  if (pc && SafeMem_IsReadable(pc, 32)) {
    return pc;
  }
  return nullptr;
}

UObject *FindLiveEngineObject(const EngineState &state, bool logSelection) {
  if (state.GEngine && SafeMem_IsReadable(state.GEngine, 32)) {
    const std::string engineName = EngineDiscovery::GetObjectNameStr(state, state.GEngine);
    if (!IsDefaultObjectName(engineName)) {
      if (logSelection) {
        char buffer[512] = {};
        snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] Using engine object: %s", EngineDiscovery::BuildFullName(state, state.GEngine).c_str());
        EngineDiscovery::WriteLogLine(buffer);
      }
      return state.GEngine;
    }
  }

  const char *preferredEngineClasses[] = {"GameEngine", "Engine"};

  for (const char *className : preferredEngineClasses) {
    UClass *engineClass = FindClassByNameExact(state, className);
    if (!engineClass) {
      char buffer[256];
      snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] FindClassByNameExact failed for %s", className);
      EngineDiscovery::WriteLogLine(buffer);
      continue;
    } else if (logSelection) {
      char buffer[256];
      snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] Found %s class at %p", className, engineClass);
      EngineDiscovery::WriteLogLine(buffer);
    }

    UObject *engineObject = FindFirstObjectOfClass(state, engineClass, false);
    if (engineObject) {
      if (logSelection) {
        char buffer[512] = {};
        snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] Using engine object: %s", EngineDiscovery::BuildFullName(state, engineObject).c_str());
        EngineDiscovery::WriteLogLine(buffer);
      }
      return engineObject;
    } else {
      char buffer[256];
      snprintf(buffer, sizeof(buffer), "[ConsoleEnabler] FindFirstObjectOfClass failed for %s class", className);
      EngineDiscovery::WriteLogLine(buffer);
    }
  }

  return nullptr;
}

UObject *ResolveGameViewport(const EngineState &state, UObject *engineObject) {
  if (!engineObject) return nullptr;

  UClass *engineClass = EngineDiscovery::GetObjectClass(state, engineObject);
  if (engineClass) {
    const int32_t gameViewportOffset = FindPropertyOffset(state, (UStruct *)engineClass, "GameViewport");
    if (gameViewportOffset >= 0) {
      UObject *gameViewport = (UObject *)SafeMem_ReadPtr((uint8_t *)engineObject + gameViewportOffset);
      if (gameViewport) {
        return gameViewport;
      }
    }
  }

  UClass *viewportBaseClass = FindClassByNameExact(state, "GameViewportClient");
  if (!viewportBaseClass) return nullptr;
  return FindFirstObjectOfClass(state, viewportBaseClass, false);
}

ConstructHelperInfo FindConstructHelper(const EngineState &state) {
  static ConstructHelperInfo cachedResult = {};
  static bool cached = false;

  if (cached) {
    return cachedResult;
  }

  static const char *kCandidateNames[] = {
      "SpawnObject", "ConstructObjectFromClass", "ConstructObject",
      "CreateObject", "Generic_CreateObject"};

  for (const char *candidateName : kCandidateNames) {
    std::vector<UObject *> fastMatches = EngineDiscovery::FindObjectsByNameFast(
        const_cast<EngineState &>(state), candidateName);

    for (UObject *fast : fastMatches) {
      if (fast && EngineDiscovery::GetObjectClass(state, fast) == state.UFunctionClass) {
        UObject *ownerClassObject = FindOwningClassObject(state, fast);
        if (ownerClassObject) {
          cachedResult.Function = fast;
          cachedResult.OwnerClass = (UClass *)ownerClassObject;
          cachedResult.Name = candidateName;
          cached = true;
          return cachedResult;
        }
      }
    }
  }

  if (state.bIndexBuilt) {
    return {};
  }

  const int32_t count = EngineDiscovery::GetObjectCount(state);
  for (const char *candidateName : kCandidateNames) {
    for (int32_t i = 0; i < count; ++i) {
      UObject *object = EngineDiscovery::GetObjectByIndex(state, i);
      if (!object) continue;
      if (EngineDiscovery::GetObjectClass(state, object) != state.UFunctionClass) continue;
      if (EngineDiscovery::GetObjectNameStr(state, object) != candidateName) continue;

      UObject *ownerClassObject = FindOwningClassObject(state, object);
      if (!ownerClassObject) continue;

      cachedResult.Function = object;
      cachedResult.OwnerClass = (UClass *)ownerClassObject;
      cachedResult.Name = candidateName;
      cached = true;
      return cachedResult;
    }
  }

  return {};
}

}
