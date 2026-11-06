#include "EncryptedCache.h"
#include "EngineDetection.h"
#include "Logger.h"
#include "ObjectArray.h"
#include "NameResolution.h"
#include "../utils/SafeMemory.h"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace EncryptedCache {

static void CipherData(uint8_t *data, size_t size, const char *gameName) {
  static const char kBaseKey[] = "AntigravityUE2026!";
  size_t baseLen = sizeof(kBaseKey) - 1;
  size_t gameLen = gameName ? strlen(gameName) : 0;

  for (size_t i = 0; i < size; i++) {
    uint8_t k1 = (uint8_t)kBaseKey[i % baseLen];
    uint8_t k2 = (gameLen > 0) ? (uint8_t)gameName[i % gameLen] : 0x5A;
    data[i] ^= (k1 ^ k2 ^ (uint8_t)(i & 0xFF));
  }
}

static bool GetPEModuleInfo(uintptr_t base, uint32_t &timestamp, uint32_t &checksum, uint32_t &imageSize) {
  if (!base) return false;
  auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
  auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(base + static_cast<DWORD>(dos->e_lfanew));
  if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

  timestamp = nt->FileHeader.TimeDateStamp;
  checksum = nt->OptionalHeader.CheckSum;
  imageSize = nt->OptionalHeader.SizeOfImage;
  return true;
}

std::wstring GetCacheFilePathW(HMODULE hModule, const char *gameName, bool createDir) {
  wchar_t dllPath[MAX_PATH] = {0};
  if (!GetModuleFileNameW(hModule, dllPath, MAX_PATH)) return L"";

  std::filesystem::path p(dllPath);
  std::filesystem::path dir = p.parent_path() / L"Cache";

  if (createDir) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
  }

  std::wstring filename;
  if (gameName && gameName[0]) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, gameName, -1, nullptr, 0);
    if (wlen > 1) {
      filename.resize(wlen - 1);
      MultiByteToWideChar(CP_UTF8, 0, gameName, -1, filename.data(), wlen);
    }
  }
  if (filename.empty()) filename = L"UniversalDiscovery";

  for (wchar_t &c : filename) {
    if (c == L':' || c == L'/' || c == L'\\' || c == L'*' ||
        c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|')
      c = L'_';
  }

  return (dir / (filename + L".umsig")).wstring();
}

static std::string WideToUtf8(const std::wstring &w) {
  if (w.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (n <= 1) return {};
  std::string s(n - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
  return s;
}

bool TryLoadSnapshot(EngineState &state, HMODULE hModule) {
  std::wstring cachePath = GetCacheFilePathW(hModule, state.GameName, false);
  if (cachePath.empty() || !std::filesystem::exists(cachePath)) {
    auto mainMod = PatternScan::GetMainModule();
    std::string altName = PatternScan::ExtractGameName(mainMod.name);
    if (!altName.empty() && altName != state.GameName) {
      std::wstring altPath = GetCacheFilePathW(hModule, altName.c_str(), false);
      if (!altPath.empty() && std::filesystem::exists(altPath)) {
        cachePath = altPath;
      }
    }
  }
  if (cachePath.empty() || !std::filesystem::exists(cachePath)) return false;

  std::ifstream file(cachePath, std::ios::binary);
  if (!file.is_open()) return false;

  SnapshotData snap = {};
  file.read(reinterpret_cast<char *>(&snap), sizeof(snap));
  if (file.gcount() != sizeof(snap)) return false;
  file.close();

  if (memcmp(snap.Header.Magic, "UDCACHE1", 8) != 0) return false;
  if (snap.Header.StructSize != sizeof(SnapshotData)) return false;

  const char *cipherName = (snap.Header.GameName[0] != '\0') ? snap.Header.GameName : state.GameName;
  uint8_t *payloadPtr = reinterpret_cast<uint8_t *>(&snap) + sizeof(SnapshotHeader);
  size_t payloadSize = sizeof(SnapshotData) - sizeof(SnapshotHeader);
  CipherData(payloadPtr, payloadSize, cipherName);

  auto mainMod = PatternScan::GetMainModule();
  uint32_t curTS = 0, curCS = 0, curIS = 0;
  if (!GetPEModuleInfo(mainMod.base, curTS, curCS, curIS)) return false;

  if (snap.Header.PETimeDateStamp != curTS || snap.Header.PEImageSize != curIS) {
    Log("[Cache] Snapshot PE metadata mismatch (stored TS=0x%X, cur TS=0x%X). Invalidation forced.",
        snap.Header.PETimeDateStamp, curTS);
    return false;
  }

  state.UEMajor = snap.Header.UEMajor;
  state.UEMinor = snap.Header.UEMinor;
  if (snap.Header.GameName[0] != '\0') {
    strncpy(state.GameName, snap.Header.GameName, sizeof(state.GameName) - 1);
    state.GameName[sizeof(state.GameName) - 1] = '\0';
  }

  state.GUObjectArray       = snap.GUObjectArray_RVA       ? reinterpret_cast<void *>(mainMod.base + snap.GUObjectArray_RVA)       : nullptr;
  state.GNames              = snap.GNames_RVA              ? reinterpret_cast<void *>(mainMod.base + snap.GNames_RVA)              : nullptr;
  state.GWorldPtr           = snap.GWorldPtr_RVA           ? reinterpret_cast<void **>(mainMod.base + snap.GWorldPtr_RVA)          : nullptr;
  if (state.GWorldPtr && IsReadable(state.GWorldPtr, sizeof(void *))) {
    state.GWorld = *state.GWorldPtr;
  }

  state.GMallocLocation     = snap.GMalloc_RVA             ? reinterpret_cast<void *>(mainMod.base + snap.GMalloc_RVA)             : nullptr;
  state.GMalloc             = nullptr;
  state.GMallocMallocSlot   = -1;
  state.GMallocName[0]      = 0;
  if (state.GMallocLocation) {
    void *mallocInstance = nullptr;
    if (SafeMem_Read(state.GMallocLocation, &mallocInstance, sizeof(mallocInstance)) && mallocInstance) {

      char liveName[64] = {};
      int nameSlot = -1;
      if (snap.GMallocName[0] &&
          EngineDiscovery::GetAllocatorDescriptiveName(mallocInstance, liveName, nameSlot) &&
          _stricmp(liveName, snap.GMallocName) == 0) {

        const int32_t cachedSlot = snap.GMallocMallocSlot;
        const bool slotUsable =
            cachedSlot >= 0 && cachedSlot < 32 &&
            EngineDiscovery::ProbeAllocatorMalloc(mallocInstance, cachedSlot, 64) != nullptr;
        if (slotUsable) {
          state.GMalloc = mallocInstance;
          state.GMallocMallocSlot = cachedSlot;
          strncpy(state.GMallocName, snap.GMallocName, sizeof(state.GMallocName) - 1);
          Log("[Cache] GMalloc restored: location %p, instance %p, allocator \"%s\", Malloc slot %d (allocation probe ok)",
              state.GMallocLocation, mallocInstance, liveName, cachedSlot);
        } else {
          if (cachedSlot >= 0 && cachedSlot < 32) {
            Log("[Cache] GMalloc identity verified but Malloc slot %d failed the allocation probe — rediscovering",
                cachedSlot);
          } else {
            Log("[Cache] GMalloc identity verified but the snapshot has no Malloc slot — rediscovering");
          }
          state.GMalloc = nullptr;
        }
      } else if (snap.GMallocName[0]) {
        Log("[Cache] GMalloc identity changed: cached \"%s\" vs live \"%s\" — rediscovering",
            snap.GMallocName, liveName[0] ? liveName : "(none)");
        state.GMalloc = nullptr;
      } else if (EngineDiscovery::IsValidMallocInstance(mallocInstance)) {

        state.GMalloc = nullptr;
        Log("[Cache] GMalloc restored from legacy snapshot without a Malloc slot; rediscovering");
      }
    }
  }
  if (state.GMalloc && state.GMallocMallocSlot < 0) {
    Log("[Cache] Cached GMalloc has no verified Malloc slot; rediscovering");
    state.GMalloc = nullptr;
  }
  if (!state.GMalloc && !EngineDiscovery::FindGMalloc(state)) {
    Log("[Cache] Restored snapshot has no valid GMalloc and rediscovery failed.");
    return false;
  }
  state.GNatives            = snap.GNatives_RVA            ? reinterpret_cast<void *>(mainMod.base + snap.GNatives_RVA)            : nullptr;
  state.GUObjectHashTables  = snap.GUObjectHashTables_RVA  ? reinterpret_cast<void *>(mainMod.base + snap.GUObjectHashTables_RVA)  : nullptr;
  state.FNameToString       = snap.FNameToString_RVA       ? reinterpret_cast<FName_ToString_t>(mainMod.base + snap.FNameToString_RVA)                              : nullptr;
  state.StaticConstructObject = snap.StaticConstructObject_RVA ? reinterpret_cast<StaticConstructObject_Internal_t>(mainMod.base + snap.StaticConstructObject_RVA)  : nullptr;
  state.bStaticConstructTakesParams = (snap.Header.UEMajor >= 5 || (snap.Header.UEMajor == 4 && snap.Header.UEMinor >= 23)) ? true : (snap.bStaticConstructTakesParams != 0);
  state.IConsoleManager     = nullptr;
  state.IConsoleManagerLocation = nullptr;
  if (snap.IConsoleManagerLocation_RVA) {
    void *managerLocation =
        reinterpret_cast<void *>(mainMod.base + snap.IConsoleManagerLocation_RVA);
    void *manager = nullptr;
    bool accepted = false;
    if (SafeMem_Read(managerLocation, &manager, sizeof(manager)) && manager &&
        SafeMem_IsCanonical(manager)) {

      const int32_t off = snap.IConsoleManager_TMapOff;
      uint8_t header[0xB0] = {};
      if (off >= 0x8 && off <= 0xA0 &&
          SafeMem_Read(manager, header, sizeof(header))) {
        void *data = nullptr;
        int32_t num = 0;
        int32_t maxNum = 0;
        memcpy(&data, header + off, sizeof(data));
        memcpy(&num, header + off + 8, sizeof(num));
        memcpy(&maxNum, header + off + 12, sizeof(maxNum));
        if (data && num > 0 && maxNum >= num && maxNum <= (1 << 24)) {
          state.IConsoleManager = manager;
          state.IConsoleManagerLocation = managerLocation;
          accepted = true;
          Log("[Cache] IConsoleManager restored: global %p, instance %p "
              "(TMap off=0x%X, commands=%d)",
              managerLocation, manager, off, num);
        }
      }
    }
    if (!accepted)
      Log("[Cache] IConsoleManager pointer invalid — rediscovering");
  }

  state.UObject_FlagsOff = snap.UObject_FlagsOff;
  state.UObject_IndexOff = snap.UObject_IndexOff;
  state.UObject_ClassOff = snap.UObject_ClassOff;
  state.UObject_NameOff = snap.UObject_NameOff;
  state.UObject_OuterOff = snap.UObject_OuterOff;

  state.UStruct_SuperOff = snap.UStruct_SuperOff;
  state.UStruct_ChildrenOff = snap.UStruct_ChildrenOff;
  state.UStruct_ChildPropsOff = snap.UStruct_ChildPropsOff;
  state.UStruct_PropertiesSizeOff = snap.UStruct_PropertiesSizeOff;
  state.UStruct_MinAlignmentOff = snap.UStruct_MinAlignmentOff;
  state.UField_NextOff = snap.UField_NextOff;

  state.FField_ClassOff = snap.FField_ClassOff;
  state.FFieldClass_NameOff = snap.FFieldClass_NameOff;
  state.FField_NameOff = snap.FField_NameOff;
  state.FField_NextOff = snap.FField_NextOff;
  state.FProperty_OffsetOff = snap.FProperty_OffsetOff;
  state.UProperty_OffsetOff = snap.UProperty_OffsetOff;

  state.FProperty_ArrayDimOff = snap.FProperty_ArrayDimOff;
  state.FProperty_ElementSizeOff = snap.FProperty_ElementSizeOff;
  state.FProperty_PropertyFlagsOff = snap.FProperty_PropertyFlagsOff;

  state.FArrayProperty_InnerOff = snap.FArrayProperty_InnerOff;
  if (((state.UEMajor == 5 && state.UEMinor >= 3) || state.UEMajor > 5) &&
      state.FBoolProperty_BaseOff > 0 && state.FArrayProperty_InnerOff == state.FBoolProperty_BaseOff) {
    state.FArrayProperty_InnerOff = state.FBoolProperty_BaseOff + 8;
  }
  state.FStructProperty_StructOff = snap.FStructProperty_StructOff;
  state.FObjectProperty_ClassOff = snap.FObjectProperty_ClassOff;
  state.FEnumProperty_UnderlyingOff = snap.FEnumProperty_UnderlyingOff;
  state.FEnumProperty_EnumOff = snap.FEnumProperty_EnumOff;
  state.FMapProperty_KeyPropOff = snap.FMapProperty_KeyPropOff;
  state.FMapProperty_ValuePropOff = snap.FMapProperty_ValuePropOff;
  state.FSetProperty_ElementPropOff = snap.FSetProperty_ElementPropOff;
  state.FOptionalProperty_ValuePropOff = snap.FOptionalProperty_ValuePropOff;
  state.FByteProperty_EnumOff = snap.FByteProperty_EnumOff;
  state.FBoolProperty_BaseOff = snap.FBoolProperty_BaseOff;
  state.FClassProperty_MetaClassOff = snap.FClassProperty_MetaClassOff;

  state.UEnum_NamesOff = snap.UEnum_NamesOff;
  state.bIsNewUE5Enum = (snap.bIsNewUE5Enum != 0);
  state.FName_IsIndirect = (snap.FName_IsIndirect != 0);
  state.FName_BlockOffsetBits = snap.FName_BlockOffsetBits;

  state.UWorld_PersistentLevelOff = snap.UWorld_PersistentLevelOff;
  state.ULevel_ActorsOff = snap.ULevel_ActorsOff;

  state.ProcessEvent_VTableIdx = snap.ProcessEvent_VTableIdx;
  state.UFunction_FlagsOff = snap.UFunction_FlagsOff;

  state.ProcessInternal = snap.ProcessInternal_RVA ? reinterpret_cast<void *>(mainMod.base + snap.ProcessInternal_RVA) : nullptr;
  state.ProcessLocalScriptFunction = snap.ProcessLocalScriptFunction_RVA ? reinterpret_cast<void *>(mainMod.base + snap.ProcessLocalScriptFunction_RVA) : nullptr;
  if (snap.UFunction_FuncOff >= 0) {
    state.UFunction_FuncOff = snap.UFunction_FuncOff;
  }
  if (state.ProcessInternal) {
    uint8_t probe = 0;
    if (!SafeMem_Read(state.ProcessInternal, &probe, sizeof(probe))) {
      state.ProcessInternal = nullptr;
    }
  }
  if (state.ProcessLocalScriptFunction) {
    uint8_t probe = 0;
    if (!SafeMem_Read(state.ProcessLocalScriptFunction, &probe, sizeof(probe))) {
      state.ProcessLocalScriptFunction = nullptr;
    }
  }
  if (state.ProcessInternal && state.ProcessLocalScriptFunction) {
    Log("[Cache] Kismet VM restored from cache: ProcessInternal=%p, ProcessLocalScriptFunction=%p, FuncOff=0x%X",
        state.ProcessInternal, state.ProcessLocalScriptFunction, state.UFunction_FuncOff);
  }

  state.FUObjectItem_Size = snap.FUObjectItem_Size;
  state.FUObjectItem_ObjectOff = snap.FUObjectItem_ObjectOff;
  state.UObjectArray_ItemSize = snap.FUObjectItem_Size;

  state.TUObjectArray_ObjectsOff = snap.TUObjectArray_ObjectsOff;
  state.TUObjectArray_NumElementsOff = snap.TUObjectArray_NumElementsOff;
  state.TUObjectArray_MaxElementsOff = snap.TUObjectArray_MaxElementsOff;

  state.FName_Size = snap.FName_Size;
  state.FName_CompIdxOff = snap.FName_CompIdxOff;
  state.FName_NumberOff = snap.FName_NumberOff;
  state.bUseCasePreservingName = (snap.bUseCasePreservingName != 0);
  state.bUseOutlineNumber = (snap.bUseOutlineNumber != 0);
  state.bUseFProperty = (snap.bUseFProperty != 0);
  state.bUseMaskForFieldOwner = (snap.bUseMaskForFieldOwner != 0);

  state.FName_ChunksStart = snap.FName_ChunksStart;
  state.FName_HeaderOff = snap.FName_HeaderOff;
  state.FName_Stride = snap.FName_Stride;
  state.FName_LenShift = snap.FName_LenShift;
  state.FName_LenMask = snap.FName_LenMask;

  state.IConsoleManager_TMapOff = snap.IConsoleManager_TMapOff;

  if (!state.GUObjectArray || EngineDiscovery::GetObjectCount(state) <= 0) {
    Log("[Cache] Restored GUObjectArray validation failed.");
    return false;
  }

  if (!state.GNames || !EngineDiscovery::IsValidFNamePool(state, state.GNames)) {
    Log("[Cache] Restored GNames validation failed.");
    return false;
  }

  state.bGlobalsFound = true;
  state.bOffsetsDiscovered = true;
  state.bFNameLayoutDiscovered = true;

  Log("[Cache] Successfully loaded encrypted discovery for %s (PE stamp: 0x%X, GUObjectArray: %p)",
      state.GameName, curTS, state.GUObjectArray);
  return true;
}

bool SaveSnapshot(const EngineState &state, HMODULE hModule) {
  if (!state.GUObjectArray || !state.GNames) return false;

  std::wstring cachePath = GetCacheFilePathW(hModule, state.GameName, true);
  if (cachePath.empty()) return false;

  auto mainMod = PatternScan::GetMainModule();
  uint32_t curTS = 0, curCS = 0, curIS = 0;
  if (!GetPEModuleInfo(mainMod.base, curTS, curCS, curIS)) return false;

  SnapshotData snap = {};
  memcpy(snap.Header.Magic, "UDCACHE1", 8);
  snap.Header.Version = 5;
  snap.Header.StructSize = sizeof(SnapshotData);
  snap.Header.PETimeDateStamp = curTS;
  snap.Header.PECheckSum = curCS;
  snap.Header.PEImageSize = curIS;
  strncpy(snap.Header.GameName, state.GameName, sizeof(snap.Header.GameName) - 1);
  snap.Header.UEMajor = state.UEMajor;
  snap.Header.UEMinor = state.UEMinor;

  auto getRVA = [&](void *ptr) -> uint64_t {
    if (!ptr) return 0;
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    if (addr >= mainMod.base && addr < mainMod.base + mainMod.size) {
      return static_cast<uint64_t>(addr - mainMod.base);
    }
    return 0;
  };

  snap.GUObjectArray_RVA       = getRVA(state.GUObjectArray);
  snap.GNames_RVA              = getRVA(state.GNames);
  snap.GWorldPtr_RVA           = getRVA(reinterpret_cast<void *>(state.GWorldPtr));

  snap.GMalloc_RVA             = getRVA(state.GMallocLocation);
  strncpy(snap.GMallocName, state.GMallocName, sizeof(snap.GMallocName) - 1);
  snap.GMallocName[sizeof(snap.GMallocName) - 1] = 0;
  snap.GMallocMallocSlot       = state.GMallocMallocSlot;
  snap.GNatives_RVA            = getRVA(state.GNatives);
  snap.GUObjectHashTables_RVA  = getRVA(state.GUObjectHashTables);
  snap.IConsoleManagerLocation_RVA = getRVA(state.IConsoleManagerLocation);
  snap.FNameToString_RVA       = getRVA(reinterpret_cast<void *>(state.FNameToString));
  snap.StaticConstructObject_RVA = getRVA(reinterpret_cast<void *>(state.StaticConstructObject));
  snap.bStaticConstructTakesParams = state.bStaticConstructTakesParams ? 1 : 0;

  snap.UObject_FlagsOff = state.UObject_FlagsOff;
  snap.UObject_IndexOff = state.UObject_IndexOff;
  snap.UObject_ClassOff = state.UObject_ClassOff;
  snap.UObject_NameOff = state.UObject_NameOff;
  snap.UObject_OuterOff = state.UObject_OuterOff;

  snap.UStruct_SuperOff = state.UStruct_SuperOff;
  snap.UStruct_ChildrenOff = state.UStruct_ChildrenOff;
  snap.UStruct_ChildPropsOff = state.UStruct_ChildPropsOff;
  snap.UStruct_PropertiesSizeOff = state.UStruct_PropertiesSizeOff;
  snap.UStruct_MinAlignmentOff = state.UStruct_MinAlignmentOff;
  snap.UField_NextOff = state.UField_NextOff;

  snap.FField_ClassOff = state.FField_ClassOff;
  snap.FFieldClass_NameOff = state.FFieldClass_NameOff;
  snap.FField_NameOff = state.FField_NameOff;
  snap.FField_NextOff = state.FField_NextOff;
  snap.FProperty_OffsetOff = state.FProperty_OffsetOff;
  snap.UProperty_OffsetOff = state.UProperty_OffsetOff;

  snap.FProperty_ArrayDimOff = state.FProperty_ArrayDimOff;
  snap.FProperty_ElementSizeOff = state.FProperty_ElementSizeOff;
  snap.FProperty_PropertyFlagsOff = state.FProperty_PropertyFlagsOff;

  snap.FArrayProperty_InnerOff = state.FArrayProperty_InnerOff;
  snap.FStructProperty_StructOff = state.FStructProperty_StructOff;
  snap.FObjectProperty_ClassOff = state.FObjectProperty_ClassOff;
  snap.FEnumProperty_UnderlyingOff = state.FEnumProperty_UnderlyingOff;
  snap.FEnumProperty_EnumOff = state.FEnumProperty_EnumOff;
  snap.FMapProperty_KeyPropOff = state.FMapProperty_KeyPropOff;
  snap.FMapProperty_ValuePropOff = state.FMapProperty_ValuePropOff;
  snap.FSetProperty_ElementPropOff = state.FSetProperty_ElementPropOff;
  snap.FOptionalProperty_ValuePropOff = state.FOptionalProperty_ValuePropOff;
  snap.FByteProperty_EnumOff = state.FByteProperty_EnumOff;
  snap.FBoolProperty_BaseOff = state.FBoolProperty_BaseOff;
  snap.FClassProperty_MetaClassOff = state.FClassProperty_MetaClassOff;

  snap.UEnum_NamesOff = state.UEnum_NamesOff;
  snap.bIsNewUE5Enum = state.bIsNewUE5Enum ? 1 : 0;
  snap.FName_IsIndirect = state.FName_IsIndirect ? 1 : 0;
  snap.FName_BlockOffsetBits = state.FName_BlockOffsetBits;

  snap.UWorld_PersistentLevelOff = state.UWorld_PersistentLevelOff;
  snap.ULevel_ActorsOff = state.ULevel_ActorsOff;

  snap.ProcessEvent_VTableIdx = state.ProcessEvent_VTableIdx;
  snap.UFunction_FlagsOff = state.UFunction_FlagsOff;

  snap.ProcessInternal_RVA = getRVA(state.ProcessInternal);
  snap.ProcessLocalScriptFunction_RVA = getRVA(state.ProcessLocalScriptFunction);
  snap.UFunction_FuncOff = state.UFunction_FuncOff;

  snap.FUObjectItem_Size = state.FUObjectItem_Size;
  snap.FUObjectItem_ObjectOff = state.FUObjectItem_ObjectOff;

  snap.TUObjectArray_ObjectsOff = state.TUObjectArray_ObjectsOff;
  snap.TUObjectArray_NumElementsOff = state.TUObjectArray_NumElementsOff;
  snap.TUObjectArray_MaxElementsOff = state.TUObjectArray_MaxElementsOff;

  snap.FName_Size = state.FName_Size;
  snap.FName_CompIdxOff = state.FName_CompIdxOff;
  snap.FName_NumberOff = state.FName_NumberOff;
  snap.bUseCasePreservingName = state.bUseCasePreservingName ? 1 : 0;
  snap.bUseOutlineNumber = state.bUseOutlineNumber ? 1 : 0;
  snap.bUseFProperty = state.bUseFProperty ? 1 : 0;
  snap.bUseMaskForFieldOwner = state.bUseMaskForFieldOwner ? 1 : 0;

  snap.FName_ChunksStart = state.FName_ChunksStart;
  snap.FName_HeaderOff = state.FName_HeaderOff;
  snap.FName_Stride = state.FName_Stride;
  snap.FName_LenShift = state.FName_LenShift;
  snap.FName_LenMask = state.FName_LenMask;

  snap.IConsoleManager_TMapOff = state.IConsoleManager_TMapOff;

  uint8_t *payloadPtr = reinterpret_cast<uint8_t *>(&snap) + sizeof(SnapshotHeader);
  size_t payloadSize = sizeof(SnapshotData) - sizeof(SnapshotHeader);
  CipherData(payloadPtr, payloadSize, state.GameName);

  std::ofstream file(cachePath, std::ios::binary);
  if (!file.is_open()) return false;
  file.write(reinterpret_cast<const char *>(&snap), sizeof(snap));
  file.close();

  Log("[Cache] Saved encrypted discovery to %s", WideToUtf8(cachePath).c_str());
  return true;
}

}
