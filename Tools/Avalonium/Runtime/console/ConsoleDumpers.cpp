#include "ConsoleInternal.h"
#include "ConsoleVariables.h"
#include "../../Helpers/minhook/src/hde/hde64.h"
#include <cstdint>
#include <set>
#include <thread>

namespace ConsoleEnabler {

static void LaunchDumpsAsync(const EngineState &state);

static bool IsChildOfStruct(const EngineState &state, UStruct *structObj, UStruct *baseStruct) {
  if (!structObj || !baseStruct) return false;
  if (structObj == baseStruct) return true;

  UStruct *current = structObj;
  for (int depth = 0; depth < 32 && current; ++depth) {
    if (current == baseStruct) return true;
    current = EngineDiscovery::GetSuperStruct(state, current);
  }
  return false;
}

constexpr uint64_t kCpfConstParm = 0x0000000000000002ULL;
constexpr uint64_t kCpfParm = 0x0000000000000080ULL;
constexpr uint64_t kCpfOutParm = 0x0000000000000100ULL;
constexpr uint64_t kCpfReturnParm = 0x0000000000000400ULL;
constexpr uint64_t kCpfReferenceParm = 0x0000000008000000ULL;
constexpr uint32_t kFuncExec = 0x00000200;

static void *ReadPointerField(void *base, int32_t offset) {
  if (!base || offset < 0) return nullptr;
  return SafeMem_ReadPtr((uint8_t *)base + offset);
}

static std::string ReflectedName(const EngineState &state, void *object) {
  if (!object) return "";
  std::string name = EngineDiscovery::GetObjectNameStr(state, (UObject *)object);
  if (name.empty() || name == "None" || name[0] == '<') return "";
  return name;
}

static std::string ClassCppName(const EngineState &state, void *classObject) {
  std::string name = ReflectedName(state, classObject);
  if (name.empty()) return "UObject";
  const bool isActor =
      EngineDiscovery::IsStructA(state, (UStruct *)classObject, (UStruct *)state.AActorClass);
  return std::string(isActor ? "A" : "U") + name;
}

static std::string ExecPropertyType(const EngineState &state, void *property, int depth) {
  if (!property || depth > 4) return "uint8";

  const std::string typeName = EngineDiscovery::GetFFieldTypeName(state, property);

  if (typeName == "BoolProperty") return "bool";
  if (typeName == "Int8Property") return "int8";
  if (typeName == "Int16Property") return "int16";
  if (typeName == "IntProperty" || typeName == "Int32Property") return "int32";
  if (typeName == "Int64Property") return "int64";
  if (typeName == "UInt16Property") return "uint16";
  if (typeName == "UInt32Property") return "uint32";
  if (typeName == "UInt64Property") return "uint64";
  if (typeName == "FloatProperty") return "float";
  if (typeName == "DoubleProperty") return "double";
  if (typeName == "NameProperty") return "FName";
  if (typeName == "StrProperty") return "FString";
  if (typeName == "Utf8StrProperty") return "FUtf8String";
  if (typeName == "AnsiStrProperty") return "FAnsiString";
  if (typeName == "TextProperty") return "FText";
  if (typeName == "FieldPathProperty") return "FFieldPath";

  if (typeName == "ByteProperty") {
    std::string enumName =
        ReflectedName(state, ReadPointerField(property, state.FByteProperty_EnumOff));
    return enumName.empty() ? "uint8" : enumName;
  }

  if (typeName == "EnumProperty") {
    std::string enumName =
        ReflectedName(state, ReadPointerField(property, state.FEnumProperty_EnumOff));
    if (!enumName.empty()) return enumName;
    return ExecPropertyType(state, ReadPointerField(property, state.FEnumProperty_UnderlyingOff),
                            depth + 1);
  }

  if (typeName == "StructProperty") {
    std::string structName =
        ReflectedName(state, ReadPointerField(property, state.FStructProperty_StructOff));
    return structName.empty() ? "FStruct" : "F" + structName;
  }

  if (typeName == "ObjectProperty" || typeName == "ObjectPtrProperty")
    return ClassCppName(state, ReadPointerField(property, state.FObjectProperty_ClassOff)) + "*";

  if (typeName == "ClassProperty" || typeName == "ClassPtrProperty") {
    void *metaClass = ReadPointerField(property, state.FClassProperty_MetaClassOff);
    if (!metaClass) metaClass = ReadPointerField(property, state.FObjectProperty_ClassOff);
    return "TSubclassOf<" + ClassCppName(state, metaClass) + ">";
  }

  if (typeName == "WeakObjectProperty")
    return "TWeakObjectPtr<" +
           ClassCppName(state, ReadPointerField(property, state.FObjectProperty_ClassOff)) + ">";
  if (typeName == "LazyObjectProperty")
    return "TLazyObjectPtr<" +
           ClassCppName(state, ReadPointerField(property, state.FObjectProperty_ClassOff)) + ">";
  if (typeName == "SoftObjectProperty")
    return "TSoftObjectPtr<" +
           ClassCppName(state, ReadPointerField(property, state.FObjectProperty_ClassOff)) + ">";
  if (typeName == "SoftClassProperty")
    return "TSoftClassPtr<" +
           ClassCppName(state, ReadPointerField(property, state.FObjectProperty_ClassOff)) + ">";

  if (typeName == "InterfaceProperty") return "FScriptInterface";
  if (typeName == "DelegateProperty") return "FScriptDelegate";
  if (typeName == "MulticastDelegateProperty" || typeName == "MulticastInlineDelegateProperty")
    return "FMulticastScriptDelegate";
  if (typeName == "MulticastSparseDelegateProperty") return "FMulticastSparseDelegate";

  if (typeName == "ArrayProperty")
    return "TArray<" +
           ExecPropertyType(state, ReadPointerField(property, state.FArrayProperty_InnerOff),
                            depth + 1) +
           ">";
  if (typeName == "SetProperty")
    return "TSet<" +
           ExecPropertyType(state, ReadPointerField(property, state.FSetProperty_ElementPropOff),
                            depth + 1) +
           ">";
  if (typeName == "MapProperty")
    return "TMap<" +
           ExecPropertyType(state, ReadPointerField(property, state.FMapProperty_KeyPropOff),
                            depth + 1) +
           ", " +
           ExecPropertyType(state, ReadPointerField(property, state.FMapProperty_ValuePropOff),
                            depth + 1) +
           ">";
  if (typeName == "OptionalProperty")
    return "TOptional<" +
           ExecPropertyType(state, ReadPointerField(property, state.FOptionalProperty_ValuePropOff),
                            depth + 1) +
           ">";

  if (typeName.size() > 8 &&
      typeName.compare(typeName.size() - 8, 8, "Property") == 0)
    return typeName.substr(0, typeName.size() - 8);
  return typeName.empty() ? "uint8" : typeName;
}

static std::string ExecFlagsText(uint32_t flags) {
  struct FlagEntry {
    uint32_t bit;
    const char *name;
  };
  static const FlagEntry entries[] = {
      {0x00000400, "Native"},
      {0x04000000, "BlueprintCallable"},
      {0x10000000, "BlueprintPure"},
      {0x08000000, "BlueprintEvent"},
      {0x00002000, "Static"},
      {0x40000000, "Const"},
      {0x00000040, "Net"},
      {0x00000080, "Reliable"},
      {0x00200000, "Server"},
      {0x01000000, "Client"},
      {0x00004000, "Multicast"},
      {0x00000004, "AuthorityOnly"},
      {0x00000008, "Cosmetic"},
      {0x20000000, "EditorOnly"},
  };

  std::string text;
  for (const FlagEntry &entry : entries) {
    if ((flags & entry.bit) == 0) continue;
    if (!text.empty()) text += ", ";
    text += entry.name;
  }
  return text;
}

static std::string ExecSignature(const EngineState &state, UStruct *function,
                                 const std::string &functionName, std::string &flagsText) {
  uint32_t functionFlags = 0;
  if (state.UFunction_FlagsOff >= 0)
    SafeMem_Read((uint8_t *)function + state.UFunction_FlagsOff, &functionFlags,
                 sizeof(functionFlags));
  flagsText = ExecFlagsText(functionFlags);

  std::vector<std::string> parameters;
  std::string returnType;

  void *child = state.bUseFProperty
                    ? EngineDiscovery::GetChildPropsHead(state, function)
                    : EngineDiscovery::GetChildrenHead(state, function);
  for (int guard = 0; child && guard < 256; ++guard) {
    uint64_t propertyFlags = 0;
    if (state.FProperty_PropertyFlagsOff >= 0)
      SafeMem_Read((uint8_t *)child + state.FProperty_PropertyFlagsOff, &propertyFlags,
                   sizeof(propertyFlags));

    const std::string propertyName =
        state.bUseFProperty ? EngineDiscovery::GetFFieldName(state, child)
                            : EngineDiscovery::GetFieldName(state, child);
    const bool isParameter =
        state.FProperty_PropertyFlagsOff < 0 || (propertyFlags & kCpfParm) != 0;
    const bool isReturn =
        (propertyFlags & kCpfReturnParm) != 0 || propertyName == "ReturnValue";

    if (isParameter) {
      std::string type = ExecPropertyType(state, child, 0);
      if (isReturn) {
        returnType = type;
      } else {
        std::string declaration;
        if ((propertyFlags & kCpfOutParm) != 0)
          declaration = "out ";
        else if ((propertyFlags & kCpfConstParm) != 0)
          declaration = "const ";
        declaration += type;
        if ((propertyFlags & kCpfReferenceParm) != 0 &&
            (propertyFlags & kCpfOutParm) == 0)
          declaration += "&";
        if (!propertyName.empty()) declaration += " " + propertyName;
        parameters.push_back(declaration);
      }
    }

    child = state.bUseFProperty ? EngineDiscovery::GetFFieldNext(state, child)
                                : EngineDiscovery::GetFieldNext(state, child);
  }

  std::string signature = functionName + "(";
  for (size_t i = 0; i < parameters.size(); ++i) {
    if (i > 0) signature += ", ";
    signature += parameters[i];
  }
  signature += ")";
  if (!returnType.empty()) signature += " -> " + returnType;
  return signature;
}

struct ExecBodyCalls {
  std::vector<uint64_t> direct;
  std::vector<uint64_t> indirect;
  bool indirectJmp = false;
  bool decoded = false;
};

static ExecBodyCalls DecodeExecBody(uint64_t fn) {
  ExecBodyCalls out;
  if (!fn) return out;

  uint8_t buf[0x900];
  if (!SafeMem_Read((void *)(uintptr_t)fn, buf, sizeof(buf))) return out;
  out.decoded = true;

  size_t off = 0;
  for (int i = 0; i < 2000 && off < sizeof(buf); ++i) {
    hde64s hs = {};
    const unsigned len = hde64_disasm(buf + off, &hs);
    if (!len || (hs.flags & F_ERROR)) break;
    if (hs.opcode == 0xC3 || hs.opcode == 0xCC) break;

    const uint64_t addr = fn + off;
    if (hs.opcode == 0xE8 && (hs.flags & F_IMM32)) {
      const int32_t rel = (int32_t)hs.imm.imm32;
      out.direct.push_back((uint64_t)((int64_t)(addr + len) + rel));
    } else if (hs.opcode == 0xFF && hs.modrm_reg == 2) {
      if (hs.modrm_mod == 3) {
        out.indirect.push_back((uint64_t)(hs.modrm_rm | (hs.rex_b ? 8 : 0)) + 1);
      } else {
        uint32_t disp = 0;
        if (hs.flags & F_DISP32) disp = hs.disp.disp32;
        else if (hs.flags & F_DISP8) disp = hs.disp.disp8;
        out.indirect.push_back(0x100000000ULL + disp);
      }
    } else if (hs.opcode == 0xFF && hs.modrm_reg == 4) {
      out.indirectJmp = true;
    }
    off += len;
  }
  return out;
}

static void ComputeExecStubs(const ExecDumpContext &ctx, std::set<std::string> &stubKeys) {
  if (ctx.nativeByCommand.empty()) return;

  std::map<uint64_t, int> freq;
  for (const auto &entry : ctx.nativeByCommand)
    if (entry.second) freq[(uint64_t)(uintptr_t)entry.second]++;

  std::map<uint64_t, ExecBodyCalls> bodies;
  for (const auto &entry : ctx.nativeByCommand) {
    const uint64_t fn = (uint64_t)(uintptr_t)entry.second;
    if (fn && !bodies.count(fn)) bodies[fn] = DecodeExecBody(fn);
  }

  std::set<uint64_t> machineryDirect;
  std::set<uint64_t> machineryIndirect;
  for (const auto &kv : bodies)
    if (freq[kv.first] >= 2) {
      for (uint64_t target : kv.second.direct) machineryDirect.insert(target);
      for (uint64_t target : kv.second.indirect) machineryIndirect.insert(target);
    }

  for (const auto &entry : ctx.nativeByCommand) {
    const uint64_t fn = (uint64_t)(uintptr_t)entry.second;
    if (!fn) continue;
    const auto it = bodies.find(fn);
    if (it == bodies.end() || !it->second.decoded) continue;

    bool stub = !it->second.indirectJmp;
    if (stub)
      for (uint64_t target : it->second.direct)
        if (!machineryDirect.count(target)) { stub = false; break; }
    if (stub)
      for (uint64_t target : it->second.indirect)
        if (!machineryIndirect.count(target)) { stub = false; break; }
    if (stub) stubKeys.insert(entry.first);
  }
}

static bool OnProcessExecObject(UObject *obj, int32_t index, void *userdata) {
  ExecDumpContext *ctx = (ExecDumpContext *)userdata;
  const EngineState &state = *(ctx->state);

  if (state.UFunction_FlagsOff < 0) return true;
  if (EngineDiscovery::GetObjectClass(state, obj) != state.UFunctionClass) return true;

  uint32_t functionFlags = 0;
  if (!SafeMem_Read((uint8_t *)obj + state.UFunction_FlagsOff, &functionFlags,
                    sizeof(functionFlags)))
    return true;
  if ((functionFlags & kFuncExec) == 0) return true;

  UObject *outer = EngineDiscovery::GetObjectOuter(state, obj);
  if (!outer) return true;

  const std::string className = EngineDiscovery::GetObjectNameStr(state, outer);
  const std::string functionName = EngineDiscovery::GetObjectNameStr(state, obj);
  if (className.empty() || functionName.empty()) return true;

  std::string flagsText;
  std::string signature = ExecSignature(state, (UStruct *)obj, functionName, flagsText);
  if (!flagsText.empty()) signature += "  [" + flagsText + "]";

  ctx->classToCommands[className].push_back(signature);
  ctx->nativeByCommand.emplace_back(className + "\x01" + signature,
                                    ReadPointerField(obj, state.UFunction_FuncOff));
  ctx->totalCount++;

  if (IsClassObject(state, outer)) {
    UStruct *super = EngineDiscovery::GetSuperStruct(state, (UStruct *)outer);
    const std::string superName = ReflectedName(state, super);
    if (!superName.empty() && superName != "Object")
      ctx->classToParent[className] = superName;
  }
  return true;
}

static bool IsModuleBackupCurrent(const std::filesystem::path &source,
                                  const std::filesystem::path &destination) {
  std::error_code ec;
  const auto sourceSize = std::filesystem::file_size(source, ec);
  if (ec) return false;

  const auto destinationSize = std::filesystem::file_size(destination, ec);
  if (ec || destinationSize != sourceSize) return false;

  const auto sourceWriteTime = std::filesystem::last_write_time(source, ec);
  if (ec) return false;

  const auto destinationWriteTime = std::filesystem::last_write_time(destination, ec);
  return !ec && destinationWriteTime == sourceWriteTime;
}

void DumpLoadedModules(const EngineState &state) {
  if (!RuntimeConfig::Load(nullptr).DumpAllDeveloperCommands)
    return;

  char consoleDir[512] = {0};
  GetConsoleDumpDirectory(state, consoleDir, sizeof(consoleDir));

  std::string outFolderStr = std::string(consoleDir) + "LoadedDLLs";

  std::thread t([outFolderStr]() {
    std::error_code ec;
    std::filesystem::create_directories(outFolderStr, ec);

    HMODULE hMods[1024];
    DWORD cbNeeded;
    HANDLE hProcess = GetCurrentProcess();

    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
      int moduleCount = cbNeeded / sizeof(HMODULE);
      char logBuf[512];
      snprintf(logBuf, sizeof(logBuf), "[Dumper] Found %d loaded modules. Checking incremental backup asynchronously in: %s", moduleCount, outFolderStr.c_str());
      EngineDiscovery::WriteLogLine(logBuf);

      int copiedCount = 0;
      int skippedCount = 0;
      for (int i = 0; i < moduleCount; i++) {
        char szModName[MAX_PATH];
        if (GetModuleFileNameExA(hProcess, hMods[i], szModName, sizeof(szModName))) {
          std::filesystem::path srcPath(szModName);

          std::string ext = srcPath.extension().string();
          for (auto &c : ext) c = (char)tolower((unsigned char)c);
          if (ext == ".exe") continue;

          std::filesystem::path destPath = std::filesystem::path(outFolderStr) / srcPath.filename();

          if (IsModuleBackupCurrent(srcPath, destPath)) {
            ++skippedCount;
            continue;
          }

          std::error_code copyError;
          if (std::filesystem::copy_file(srcPath, destPath,
                                         std::filesystem::copy_options::overwrite_existing,
                                         copyError)) {
            ++copiedCount;
          }
        }
      }
      snprintf(logBuf, sizeof(logBuf),
               "[Dumper] Modules complete. Copied %d, skipped %d unchanged module(s).",
               copiedCount, skippedCount);
      EngineDiscovery::WriteLogLine(logBuf);
    }
  });
  t.join();
}

void DumpExecCommands(const EngineState &state) {
  if (!RuntimeConfig::Load(nullptr).DumpAllDeveloperCommands)
    return;

  ExecDumpContext ctx;
  ctx.state = &state;

  EngineDiscovery::ForEachObject(state, OnProcessExecObject, &ctx);

  std::set<std::string> stubKeys;
  ComputeExecStubs(ctx, stubKeys);

  char consoleDir[512] = {0};
  GetConsoleDumpDirectory(state, consoleDir, sizeof(consoleDir));

  char dumpPath[512] = {};
  snprintf(dumpPath, sizeof(dumpPath), "%sExecCommands.txt", consoleDir);

  if (ctx.totalCount == 0) {
    std::error_code ec;
    if (std::filesystem::remove(dumpPath, ec)) {
      char msg[512] = {};
      snprintf(msg, sizeof(msg), "[ConsoleEnabler] No Exec commands found; removed stale %s",
               dumpPath);
      EngineDiscovery::WriteLogLine(msg);
    }
    return;
  }

  FILE *fp = fopen(dumpPath, "w");
  if (fp) {
    fprintf(fp, "=== Avalonium - Exec Commands ===\n");
    fprintf(fp, "Game: %s\n", state.GameName);
    if (state.UEMajor > 0)
      fprintf(fp, "Engine: %d.%d\n", state.UEMajor, state.UEMinor);
    fprintf(fp, "Exec functions: %d in %d classes\n", ctx.totalCount,
            (int)ctx.classToCommands.size());
    fprintf(fp, "Stripped bodies ([Stub]): %d\n\n", (int)stubKeys.size());
    fprintf(fp,
            "Exec functions are UFUNCTION(Exec) declarations recovered from reflection,\n"
            "grouped by the class that declares them. Type the command name in the console;\n"
            "arguments follow in parentheses. The console routes a command through the\n"
            "objects around the local player (player input, controller, pawn, HUD, cheat\n"
            "manager, game mode, game state, camera manager, game instance, viewport) and\n"
            "the engine's global exec handlers, so a command runs while its declaring\n"
            "object is part of that path.\n"
            "[Stub] marks a command whose native body was compiled out of this build: the\n"
            "generated thunk still parses arguments, so the command resolves, but it calls\n"
            "Shipping builds commonly strip cheat and\n"
            "debug bodies this way while keeping the declarations in reflection.\n\n");

    for (auto &pair : ctx.classToCommands) {
      std::sort(pair.second.begin(), pair.second.end());
      pair.second.erase(std::unique(pair.second.begin(), pair.second.end()), pair.second.end());

      auto parent = ctx.classToParent.find(pair.first);
      if (parent != ctx.classToParent.end()) {
        fprintf(fp, "Class: %s (%s, %d commands)\n", pair.first.c_str(),
                parent->second.c_str(), (int)pair.second.size());
      } else {
        fprintf(fp, "Class: %s (%d commands)\n", pair.first.c_str(),
                (int)pair.second.size());
      }
      for (const std::string &command : pair.second) {
        const bool stub = stubKeys.count(pair.first + "\x01" + command) != 0;
        fprintf(fp, "  - %s%s\n", command.c_str(), stub ? "  [Stub]" : "");
      }
      fprintf(fp, "\n");
    }

    fclose(fp);
    char msg[512] = {};
    snprintf(msg, sizeof(msg), "[ConsoleEnabler] Dumped %d Exec commands to %s", ctx.totalCount,
             dumpPath);
    EngineDiscovery::WriteLogLine(msg);
  }
}

static bool OnProcessSpawnableObject(UObject *obj, int32_t index, void *userdata) {
  SpawnableDumpContext *ctx = (SpawnableDumpContext *)userdata;
  const EngineState &state = *(ctx->state);

  UClass *cls = EngineDiscovery::GetObjectClass(state, obj);
  if (!cls) return true;

  if (state.UFunctionClass && cls == state.UFunctionClass) return true;

  std::string objName = EngineDiscovery::GetObjectNameStr(state, obj);
  if (objName.empty()) return true;

  if (objName.rfind("SKEL_", 0) == 0 || objName.rfind("REINST_", 0) == 0 ||
      objName.rfind("TRASH_", 0) == 0 || objName.rfind("Default__", 0) == 0 ||
      objName.rfind("BOOL_", 0) == 0 || objName.rfind("ENUM_", 0) == 0 ||
      objName.rfind("STRUCT_", 0) == 0) {
    return true;
  }

  std::string clsName = EngineDiscovery::GetObjectNameStr(state, (UObject *)cls);

  bool isClass = false;
  if (state.UClassClass && (cls == state.UClassClass || IsChildOfStruct(state, (UStruct*)cls, (UStruct*)state.UClassClass))) {
    isClass = true;
  } else if (clsName.find("Class") != std::string::npos) {
    isClass = true;
  }

  if (isClass) {
    char fullBuf[1024] = {0};
    GetObjectFullNameSafe(obj, fullBuf, sizeof(fullBuf));
    std::string fullName = fullBuf[0] ? fullBuf : objName;

    SpawnableClassEntry entry = { objName, fullName };

    if (ctx->actorClass && IsChildOfStruct(state, (UStruct*)obj, (UStruct*)ctx->actorClass)) {
      ctx->actors.push_back(entry);
    } else if (ctx->componentClass && IsChildOfStruct(state, (UStruct*)obj, (UStruct*)ctx->componentClass)) {
      ctx->components.push_back(entry);
    } else if (ctx->widgetClass && IsChildOfStruct(state, (UStruct*)obj, (UStruct*)ctx->widgetClass)) {
      ctx->widgets.push_back(entry);
    } else {
      ctx->others.push_back(entry);
    }
    ctx->totalCount++;
  }

  return true;
}

void DumpSpawnableClasses(const EngineState &state) {
  if (!RuntimeConfig::Load(nullptr).DumpAllDeveloperCommands) return;

  SpawnableDumpContext ctx;
  ctx.state = &state;
  ctx.actorClass = state.AActorClass ? state.AActorClass : FindClassByNameExact(state, "Actor");
  ctx.componentClass = FindClassByNameExact(state, "ActorComponent");
  ctx.widgetClass = FindClassByNameExact(state, "UserWidget");

  EngineDiscovery::ForEachObject(state, OnProcessSpawnableObject, &ctx);

  if (ctx.totalCount == 0) return;

  auto entryComparator = [](const SpawnableClassEntry &a, const SpawnableClassEntry &b) {
    return a.shortName < b.shortName;
  };

  std::sort(ctx.actors.begin(), ctx.actors.end(), entryComparator);
  std::sort(ctx.components.begin(), ctx.components.end(), entryComparator);
  std::sort(ctx.widgets.begin(), ctx.widgets.end(), entryComparator);
  std::sort(ctx.others.begin(), ctx.others.end(), entryComparator);

  char consoleDir[512] = {0};
  GetConsoleDumpDirectory(state, consoleDir, sizeof(consoleDir));

  char dumpPath[512] = {};
  snprintf(dumpPath, sizeof(dumpPath), "%sSpawnableClasses.txt", consoleDir);

  FILE *fp = fopen(dumpPath, "w");
  if (fp) {
    fprintf(fp, "=== Avalonium - Spawnable Classes & Objects ===\n");
    fprintf(fp, "Game: %s\n", state.GameName);
    fprintf(fp, "Total Classes: %d (Actors: %zu, Components: %zu, Widgets: %zu, Other: %zu)\n\n",
            ctx.totalCount, ctx.actors.size(), ctx.components.size(), ctx.widgets.size(), ctx.others.size());

    if (!ctx.actors.empty()) {
      fprintf(fp, "→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←\n");
      fprintf(fp, "=== ACTOR CLASSES (AActor) [%zu] ===\n", ctx.actors.size());
      fprintf(fp, "→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←\n");
      for (const auto &entry : ctx.actors) {
        fprintf(fp, "%-45s | %s\n", entry.shortName.c_str(), entry.fullName.c_str());
      }
      fprintf(fp, "\n");
    }

    if (!ctx.components.empty()) {
      fprintf(fp, "→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←\n");
      fprintf(fp, "=== COMPONENT CLASSES (UActorComponent) [%zu] ===\n", ctx.components.size());
      fprintf(fp, "→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←\n");
      for (const auto &entry : ctx.components) {
        fprintf(fp, "%-45s | %s\n", entry.shortName.c_str(), entry.fullName.c_str());
      }
      fprintf(fp, "\n");
    }

    if (!ctx.widgets.empty()) {
      fprintf(fp, "→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←\n");
      fprintf(fp, "=== WIDGET / UI CLASSES (UUserWidget) [%zu] ===\n", ctx.widgets.size());
      fprintf(fp, "→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←\n");
      for (const auto &entry : ctx.widgets) {
        fprintf(fp, "%-45s | %s\n", entry.shortName.c_str(), entry.fullName.c_str());
      }
      fprintf(fp, "\n");
    }

    if (!ctx.others.empty()) {
      fprintf(fp, "→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←\n");
      fprintf(fp, "=== OTHER CLASSES (DataAssets / Blueprint Objects) [%zu] ===\n", ctx.others.size());
      fprintf(fp, "→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←\n");
      for (const auto &entry : ctx.others) {
        fprintf(fp, "%-45s | %s\n", entry.shortName.c_str(), entry.fullName.c_str());
      }
      fprintf(fp, "\n");
    }

    fclose(fp);
    char msg[512] = {};
    snprintf(msg, sizeof(msg), "[ConsoleEnabler] Dumped %d Spawnable/Game Classes to %s", ctx.totalCount, dumpPath);
    EngineDiscovery::WriteLogLine(msg);
  }
}

void UnlockConsoleCommands(const EngineState &state) {
  if (!state.IConsoleManager) return;

  const RuntimeFeatureConfig &runtimeCfg = RuntimeConfig::Load(nullptr);
  if (!runtimeCfg.DumpAllDeveloperCommands) {
    return;
  }

  SafeMem_SetFastMode(1);

  int32_t tmapOff = state.IConsoleManager_TMapOff > 0 ? state.IConsoleManager_TMapOff : 0x08;

  uint8_t *consoleManager = (uint8_t *)state.IConsoleManager;
  void *dataPtr = *(void **)(consoleManager + tmapOff + 0x00);
  int32_t arrayNum = *(int32_t *)(consoleManager + tmapOff + 0x08);

  uint8_t *bitArrayBase = consoleManager + tmapOff + 0x10;

  uint32_t *allocationFlags = nullptr;
  bool usingHeap = false;

  int32_t arrayMax = *(int32_t *)(consoleManager + tmapOff + 0x0C);

  uint32_t *heapPtrInline = *(uint32_t **)(bitArrayBase + 0x10);
  int32_t numBitsInline = *(int32_t *)(bitArrayBase + 0x18);

  uint32_t *heapPtrDefault = *(uint32_t **)(bitArrayBase + 0x00);
  int32_t numBitsDefault = *(int32_t *)(bitArrayBase + 0x08);

  auto IsValidHeapPtr = [](uint32_t* ptr) {
      return ptr && (uintptr_t)ptr > 0x10000 && (uintptr_t)ptr < 0x7FFFFFFFFFFFULL && SafeMem_IsReadable(ptr, 16);
  };

  if (numBitsInline == arrayNum || numBitsInline == arrayMax) {
      if (IsValidHeapPtr(heapPtrInline)) {
          allocationFlags = heapPtrInline;
          usingHeap = true;
      } else if (arrayMax <= 128) {
          allocationFlags = (uint32_t *)bitArrayBase;
          usingHeap = false;
      }
  }

  if (!allocationFlags && (numBitsDefault == arrayNum || numBitsDefault == arrayMax)) {
      if (IsValidHeapPtr(heapPtrDefault)) {
          allocationFlags = heapPtrDefault;
          usingHeap = true;
      }
  }

  if (!allocationFlags) {
      if (IsValidHeapPtr(heapPtrInline)) {
          allocationFlags = heapPtrInline;
          usingHeap = true;
      } else if (IsValidHeapPtr(heapPtrDefault)) {
          allocationFlags = heapPtrDefault;
          usingHeap = true;
      } else if (arrayMax <= 128) {
          allocationFlags = (uint32_t *)bitArrayBase;
          usingHeap = false;
      }
  }

  if (!dataPtr || arrayNum <= 0 || arrayNum > 200000 || !allocationFlags) {
    EngineDiscovery::WriteLogLine("[ConsoleEnabler] IConsoleManager TMap / AllocationFlags invalid — skipping unlock.");
    return;
  }

  const int32_t stride = 0x20;
  int unlockedCount = 0;
  int exceptionCount = 0;

  int unlockedCapacity = 1024;
  char *unlockedNames = (char *)malloc((size_t)unlockedCapacity * 128);

  int dumpCount = 0;

  char allCmdsFilePath[512] = {0};

  FILE *allCmdsFile = nullptr;
  int validCommandCount = 0;

  if (runtimeCfg.DumpAllDeveloperCommands) {
    char consoleDir[512] = {0};
    GetConsoleDumpDirectory(state, consoleDir, sizeof(consoleDir));

    snprintf(allCmdsFilePath, sizeof(allCmdsFilePath), "%sAllConsoleCommands.json", consoleDir);

    allCmdsFile = fopen(allCmdsFilePath, "w");
    if (allCmdsFile) {
      fprintf(allCmdsFile, "{\n");
      fprintf(allCmdsFile, "  \"game\": \"%s\",\n", state.GameName);
      fprintf(allCmdsFile, "  \"totalCommands\": %d,\n", arrayNum);
      fprintf(allCmdsFile, "  \"commands\": [\n");
    }
  }

  DumpConsoleVariables(state, dataPtr, allocationFlags, arrayNum, stride);

  for (int32_t i = 0; i < arrayNum; ++i) {
    int dwordIndex = i / 32;
    int bitMask = 1 << (i % 32);

    if (dwordIndex > (200000 / 32)) break;

    __try {
      if ((allocationFlags[dwordIndex] & bitMask) != 0) {
        uint8_t *element = (uint8_t *)dataPtr + (i * stride);
        void *consoleObject = *(void **)(element + 0x10);
        const ConsoleObjectSlots consoleSlots = ResolveConsoleObjectSlots(state, consoleObject);

        if (allCmdsFile) {
          void *strData = *(void **)element;
          int32_t strLen = *(int32_t *)(element + 8);
          if (strData && strLen > 0 && strLen < 128) {
            char nameBuf[128] = {0};
            for (int k = 0; k < strLen && k < 127; ++k) {
              nameBuf[k] = (char)((wchar_t *)strData)[k];
            }

            char helpBuf[1024] = {0};
            strcpy(helpBuf, "No description available.");

            if (consoleObject && consoleSlots.help >= 0 && SafeMem_IsReadable(consoleObject, 64)) {
              void **vtable = *(void ***)consoleObject;
              if (SafeMem_IsReadable(vtable, sizeof(void *) * (consoleSlots.help + 1))) {
                typedef const wchar_t *(*GetHelp_t)(void *);
                GetHelp_t GetHelp = (GetHelp_t)vtable[consoleSlots.help];
                if ((uintptr_t)GetHelp > 0x10000 && (uintptr_t)GetHelp < 0x7FFFFFFFFFFFULL) {
                  const wchar_t *helpStr = nullptr;
                  __try {
                    helpStr = GetHelp(consoleObject);
                  } __except (1) {
                    helpStr = nullptr;
                  }

                  if (helpStr && SafeMem_IsReadable((void *)helpStr, 2)) {
                    bool isEmpty = true;
                    for (int k = 0; k < 1023; ++k) {
                      wchar_t c = helpStr[k];
                      if (!c) {
                        helpBuf[k] = '\0';
                        break;
                      }
                      isEmpty = false;
                      char outChar = (char)c;
                      if (outChar == '\n' || outChar == '\r') outChar = ' ';
                      helpBuf[k] = outChar;
                    }
                    if (isEmpty) {
                      strcpy(helpBuf, "Description was stripped by developer.");
                    }
                  } else {
                    strcpy(helpBuf, "Description was stripped by developer.");
                  }
                }
              }
            }

            char escapedHelp[4096] = {0};
            int esc = 0;
            for (int h = 0; helpBuf[h] != '\0' && esc < (int)sizeof(escapedHelp) - 10; ++h) {
              char c = helpBuf[h];
              if (c == '"') {
                escapedHelp[esc++] = '\\';
                escapedHelp[esc++] = '"';
              } else if (c == '\\') {
                escapedHelp[esc++] = '\\';
                escapedHelp[esc++] = '\\';
              } else if (c == '\n') {
                escapedHelp[esc++] = '\\';
                escapedHelp[esc++] = 'n';
              } else if (c == '\r') {
                escapedHelp[esc++] = '\\';
                escapedHelp[esc++] = 'r';
              } else if (c == '\t') {
                escapedHelp[esc++] = '\\';
                escapedHelp[esc++] = 't';
              } else {
                escapedHelp[esc++] = c;
              }
            }

            if (validCommandCount > 0) {
              fprintf(allCmdsFile, ",\n");
            }
            fprintf(allCmdsFile, "    {\n");
            fprintf(allCmdsFile, "      \"command\": \"%s\",\n", nameBuf);
            fprintf(allCmdsFile, "      \"description\": \"%s\"\n", escapedHelp);
            fprintf(allCmdsFile, "    }");
            validCommandCount++;
          }
        }

        if (consoleObject && SafeMem_IsReadable(consoleObject, 64)) {
          void **vtable = *(void ***)consoleObject;

          if (dumpCount < 5) {
            char dumpBuf[512] = {0};
            int pos = 0;
            pos += snprintf(dumpBuf + pos, sizeof(dumpBuf) - pos, "[ConsoleEnabler] Obj Dump %d: ", dumpCount);
            uint32_t *mem32 = (uint32_t *)consoleObject;
            for (int k = 0; k < 12; ++k) {
              pos += snprintf(dumpBuf + pos, sizeof(dumpBuf) - pos, "%08X ", mem32[k]);
            }
            dumpCount++;
          }

          static const uint32_t kECVF_Cheat = 0x01;
          bool clearedCheat = false;
          static bool flagsAccessFaulted = false;

          if (runtimeCfg.DumpAllDeveloperCommands && consoleObject && !flagsAccessFaulted && vtable &&
              consoleSlots.flags >= 0 && consoleSlots.setFlags >= 0) {
            const int highestSlot = (consoleSlots.flags > consoleSlots.setFlags) ? consoleSlots.flags : consoleSlots.setFlags;
            if (SafeMem_IsReadable(vtable, sizeof(void *) * (highestSlot + 1))) {
              typedef uint32_t (*GetFlags_t)(const void *);
              typedef void (*SetFlags_t)(void *, uint32_t);
              GetFlags_t GetFlags = (GetFlags_t)vtable[consoleSlots.flags];
              SetFlags_t SetFlags = (SetFlags_t)vtable[consoleSlots.setFlags];

              if ((uintptr_t)GetFlags > 0x10000 && (uintptr_t)GetFlags < 0x7FFFFFFFFFFFULL &&
                  (uintptr_t)SetFlags > 0x10000 && (uintptr_t)SetFlags < 0x7FFFFFFFFFFFULL) {
                uint32_t flags = 0;
                bool gotFlags = false;
                __try {
                  flags = GetFlags(consoleObject);
                  gotFlags = true;
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                  gotFlags = false;
                }
                if (!gotFlags) {
                  flagsAccessFaulted = true;
                  EngineDiscovery::WriteLogLine("[ConsoleEnabler] Console flag accessor faulted; cheat bits left unchanged.");
                }

                if (gotFlags && (flags & kECVF_Cheat) != 0) {
                  bool wrote = false;
                  __try {
                    SetFlags(consoleObject, flags & ~kECVF_Cheat);
                    wrote = true;
                  } __except (EXCEPTION_EXECUTE_HANDLER) {
                    wrote = false;
                  }
                  if (!wrote) {
                    flagsAccessFaulted = true;
                    EngineDiscovery::WriteLogLine("[ConsoleEnabler] Console flag setter faulted; cheat bits left unchanged.");
                  }
                  clearedCheat = wrote;
                }
              }
            }
          }

          if (clearedCheat) {
            if (unlockedCount >= unlockedCapacity) {
              int newCapacity = unlockedCapacity * 2;
              char *grown = (char *)realloc(unlockedNames, (size_t)newCapacity * 128);
              if (grown) {
                memset(grown + (size_t)unlockedCapacity * 128, 0, (size_t)(newCapacity - unlockedCapacity) * 128);
                unlockedNames = grown;
                unlockedCapacity = newCapacity;
              }
            }

            if (unlockedCount < unlockedCapacity) {
              char *nameBuf = unlockedNames + ((size_t)unlockedCount * 128);
              void *strData = *(void **)element;
              int32_t strLen = *(int32_t *)(element + 8);
              if (strData && strLen > 0 && strLen < 128) {
                for (int k = 0; k < strLen && k < 127; ++k) {
                  nameBuf[k] = (char)((wchar_t *)strData)[k];
                }
              } else {
                strcpy(nameBuf, "<unknown_command>");
              }
            }
            unlockedCount++;
          }
        }
      }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      exceptionCount++;
    }
  }

  if (allCmdsFile) {
    fprintf(allCmdsFile, "\n  ]\n}\n");
    fclose(allCmdsFile);
    char logBuf[512];
    snprintf(logBuf, sizeof(logBuf), "[ConsoleEnabler] Dumped %d compiled commands to %s", validCommandCount, allCmdsFilePath);
    EngineDiscovery::WriteLogLine(logBuf);
  }

  if (unlockedCount > 0 && runtimeCfg.DumpAllDeveloperCommands) {
    char consoleDir[512] = {0};
    GetConsoleDumpDirectory(state, consoleDir, sizeof(consoleDir));

    char dumpPath[512] = {};
    snprintf(dumpPath, sizeof(dumpPath), "%sUnlockedCommands.txt", consoleDir);

    FILE *fp = fopen(dumpPath, "w");
    if (fp) {
      fprintf(fp, "=== Avalonium - Unlocked Console Commands ===\n");
      fprintf(fp, "Game: %s\n", state.GameName);
      fprintf(fp, "Total unlocked: %d\n\n", unlockedCount);

      for (int i = 0; i < unlockedCount; ++i) {
        fprintf(fp, "%s\n", unlockedNames + ((size_t)i * 128));
      }
      fclose(fp);

      char msg[512] = {};
      snprintf(msg, sizeof(msg), "[ConsoleEnabler] Dumped %d unlocked commands to %s", unlockedCount, dumpPath);
      EngineDiscovery::WriteLogLine(msg);
    }
  }

  free(unlockedNames);

  if (runtimeCfg.DumpAllDeveloperCommands) {
    char buffer[256] = {};
    if (exceptionCount > 0) {
      snprintf(buffer, sizeof(buffer),
               "[ConsoleEnabler] Processed IConsoleManager. Unlocked %d ECVF_Cheat commands (%d elements threw exceptions!).",
               unlockedCount, exceptionCount);
    } else {
      snprintf(buffer, sizeof(buffer),
               "[ConsoleEnabler] Processed IConsoleManager. Unlocked %d ECVF_Cheat commands.",
               unlockedCount);
    }
    EngineDiscovery::WriteLogLine(buffer);
  }

  LaunchDumpsAsync(state);

  SafeMem_SetFastMode(0);
}

static void LaunchDumpsAsync(const EngineState &state) {
  std::thread t1([state]() { DumpExecCommands(state); });
  std::thread t2([state]() { DumpSpawnableClasses(state); });
  std::thread t3([state]() { DumpLoadedModules(state); });
  if (t1.joinable()) t1.join();
  if (t2.joinable()) t2.join();
  if (t3.joinable()) t3.join();
}

}
