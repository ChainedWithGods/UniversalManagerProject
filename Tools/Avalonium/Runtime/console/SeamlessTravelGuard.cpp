#include "SeamlessTravelGuard.h"

#include "../core/Types.h"
#include "../core/BlueprintDiscovery.h"
#include "../core/EngineDiscovery.h"
#include "../core/FunctionFinder.h"
#include "../core/ObjectArray.h"
#include "../core/WorldDiscovery.h"
#include "ConsoleInternal.h"
#include "../utils/SafeMemory.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace SeamlessTravelGuard {
namespace {

constexpr int kMaxVtableSlots = 512;
constexpr size_t kCodeProbeBytes = 48;
constexpr size_t kStubProbeBytes = 8;

EngineState *g_State = nullptr;
bool g_Ready = false;

UClass *g_ControllerClass = nullptr;
UClass *g_PlayerControllerClass = nullptr;
UClass *g_LocalPlayerClass = nullptr;
UClass *g_PlayerClass = nullptr;

int32_t g_PlayerOffset = -1;
int32_t g_PlayerStateOffset = -1;
int32_t g_PlayerControllerOffset = -1;
int32_t g_AuthorityGameModeOffset = -1;

int g_ParticipationSlot = -1;
int g_SwitchControllerSlot = -1;
void *g_ParticipationStub = nullptr;

int g_TotalPatched = 0;
bool g_LoggedResolveFailure = false;
bool g_LoggedDiscoveryFailure = false;

thread_local bool t_Repairing = false;

void LogLine(const char *text) { EngineDiscovery::WriteLogLine(text); }

void *ReadPtrAt(const void *base, size_t offset) {
    if (!base) return nullptr;
    const uint8_t *address = static_cast<const uint8_t *>(base) + offset;
    if (!SafeMem_IsReadable(address, sizeof(void *))) return nullptr;
    return SafeMem_ReadPtr(address);
}

bool ReadBytes(const void *address, uint8_t *out, size_t size) {
    return address && out && size && SafeMem_Read(address, out, size) != 0;
}

bool IsConstantTrueStub(const uint8_t *code) {
    if (code[0] == 0xB0 && code[1] == 0x01 && code[2] == 0xC3) return true;
    return code[0] == 0xB8 && code[1] == 0x01 && code[2] == 0x00 &&
           code[3] == 0x00 && code[4] == 0x00 && code[5] == 0xC3;
}

bool ReadsPlayerStateForParticipation(const uint8_t *code) {
    const int32_t playerStateOffset = g_PlayerStateOffset;
    for (size_t i = 0; i + 8 <= kCodeProbeBytes && i < 24; ++i) {
        if (code[i] == 0x48 && code[i + 1] == 0x83 && code[i + 2] == 0xB9) {
            int32_t displacement = 0;
            std::memcpy(&displacement, code + i + 3, sizeof(displacement));
            if (displacement != playerStateOffset || code[i + 7] != 0x00) continue;
            for (size_t j = i + 8; j + 3 <= kCodeProbeBytes && j < i + 28; ++j) {
                if (code[j] == 0x0F && code[j + 1] == 0x95 && code[j + 2] == 0xC0) return true;
            }
        }
        if (code[i] == 0x48 && code[i + 1] == 0x8B && (code[i + 2] == 0x81 || code[i + 2] == 0x89)) {
            int32_t displacement = 0;
            std::memcpy(&displacement, code + i + 3, sizeof(displacement));
            if (displacement != playerStateOffset) continue;
            for (size_t j = i + 7; j + 6 <= kCodeProbeBytes && j < i + 24; ++j) {
                const bool tests = code[j] == 0x48 && code[j + 1] == 0x85 &&
                                   (code[j + 2] == 0xC0 || code[j + 2] == 0xC9);
                if (!tests) continue;
                for (size_t k = j + 3; k + 3 <= kCodeProbeBytes && k < j + 16; ++k) {
                    if (code[k] == 0x0F && code[k + 1] == 0x95 && code[k + 2] == 0xC0) return true;
                }
            }
        }
    }
    return false;
}

bool WritesPlayerControllerToPlayer(const uint8_t *code) {
    bool readsController = false;
    for (size_t i = 0; i + 4 <= 28; ++i) {
        if (code[i] == 0x48 && code[i + 1] == 0x8B && code[i + 2] == 0x41 &&
            g_PlayerControllerOffset >= 0 && g_PlayerControllerOffset <= 0xFF &&
            code[i + 3] == static_cast<uint8_t>(g_PlayerControllerOffset)) {
            readsController = true;
            break;
        }
        if (i + 7 <= 28 && code[i] == 0x48 && code[i + 1] == 0x8B && code[i + 2] == 0x81) {
            int32_t displacement = 0;
            std::memcpy(&displacement, code + i + 3, sizeof(displacement));
            if (displacement == g_PlayerControllerOffset) {
                readsController = true;
                break;
            }
        }
    }
    if (!readsController) return false;
    for (size_t i = 0; i + 8 <= 40; ++i) {
        if (code[i] != 0x48 || code[i + 1] != 0xC7) continue;
        if (code[i + 2] != 0x80 && code[i + 2] != 0x81 && code[i + 2] != 0x82) continue;
        int32_t displacement = 0;
        std::memcpy(&displacement, code + i + 3, sizeof(displacement));
        if (displacement == g_PlayerOffset) return true;
    }
    return false;
}

bool ResolveInterfaces(EngineState &state) {
    g_ControllerClass = ConsoleEnabler::FindClassByNameExact(state, "Controller");
    g_PlayerControllerClass = ConsoleEnabler::FindClassByNameExact(state, "PlayerController");
    g_LocalPlayerClass = ConsoleEnabler::FindClassByNameExact(state, "LocalPlayer");
    g_PlayerClass = ConsoleEnabler::FindClassByNameExact(state, "Player");
    if (!g_ControllerClass || !g_PlayerControllerClass || !g_LocalPlayerClass || !g_PlayerClass) {
        if (!g_LoggedResolveFailure) {
            g_LoggedResolveFailure = true;
            LogLine("[TravelGuard] Controller/PlayerController/LocalPlayer/Player classes not resolved; guard disabled.");
        }
        return false;
    }

    g_PlayerOffset = ConsoleEnabler::FindPropertyOffset(state, reinterpret_cast<UStruct *>(g_PlayerControllerClass), "Player");
    g_PlayerStateOffset = ConsoleEnabler::FindPropertyOffset(state, reinterpret_cast<UStruct *>(g_ControllerClass), "PlayerState");
    g_PlayerControllerOffset = ConsoleEnabler::FindPropertyOffset(state, reinterpret_cast<UStruct *>(g_PlayerClass), "PlayerController");
    g_AuthorityGameModeOffset = state.UWorldClass
        ? ConsoleEnabler::FindPropertyOffset(state, reinterpret_cast<UStruct *>(state.UWorldClass), "AuthorityGameMode")
        : -1;
    if (g_PlayerOffset < 0 || g_PlayerStateOffset < 0 || g_PlayerControllerOffset < 0 ||
        g_AuthorityGameModeOffset < 0 || state.UObject_OuterOff < 0) {
        if (!g_LoggedResolveFailure) {
            g_LoggedResolveFailure = true;
            char buffer[320] = {};
            std::snprintf(buffer, sizeof(buffer),
                          "[TravelGuard] Required offsets unresolved (Player=%d PlayerState=%d PlayerController=%d "
                          "AuthorityGameMode=%d Outer=%d); guard disabled.",
                          g_PlayerOffset, g_PlayerStateOffset, g_PlayerControllerOffset, g_AuthorityGameModeOffset,
                          state.UObject_OuterOff);
            LogLine(buffer);
        }
        return false;
    }
    return true;
}

bool DiscoverParticipationSlot() {
    UObject *controllerCdo = EngineDiscovery::FindClassDefaultObject(*g_State, g_ControllerClass);
    UObject *playerControllerCdo = EngineDiscovery::FindClassDefaultObject(*g_State, g_PlayerControllerClass);
    void *controllerVtable = ReadPtrAt(controllerCdo, 0);
    void *playerControllerVtable = ReadPtrAt(playerControllerCdo, 0);
    if (!controllerVtable || !playerControllerVtable) return false;

    int matches = 0;
    int foundSlot = -1;
    void *foundStub = nullptr;
    for (int slot = 0; slot < kMaxVtableSlots; ++slot) {
        void *baseFunction = ReadPtrAt(controllerVtable, static_cast<size_t>(slot) * sizeof(void *));
        void *overrideFunction = ReadPtrAt(playerControllerVtable, static_cast<size_t>(slot) * sizeof(void *));
        if (!baseFunction || !overrideFunction) continue;

        uint8_t baseCode[kCodeProbeBytes] = {};
        uint8_t overrideCode[kStubProbeBytes] = {};
        if (!ReadBytes(baseFunction, baseCode, sizeof(baseCode)) ||
            !ReadBytes(overrideFunction, overrideCode, sizeof(overrideCode))) {
            continue;
        }
        if (!ReadsPlayerStateForParticipation(baseCode)) continue;
        if (!IsConstantTrueStub(overrideCode)) continue;

        ++matches;
        foundSlot = slot;
        foundStub = overrideFunction;
    }
    if (matches != 1) return false;

    g_ParticipationSlot = foundSlot;
    g_ParticipationStub = foundStub;
    return true;
}

bool DiscoverSwitchControllerSlot() {
    UObject *localPlayerCdo = EngineDiscovery::FindClassDefaultObject(*g_State, g_LocalPlayerClass);
    void *vtable = ReadPtrAt(localPlayerCdo, 0);
    if (!vtable) return false;

    int matches = 0;
    int foundSlot = -1;
    for (int slot = 0; slot < kMaxVtableSlots; ++slot) {
        void *function = ReadPtrAt(vtable, static_cast<size_t>(slot) * sizeof(void *));
        if (!function) continue;
        uint8_t code[kCodeProbeBytes] = {};
        if (!ReadBytes(function, code, sizeof(code))) continue;
        if (!WritesPlayerControllerToPlayer(code)) continue;
        ++matches;
        foundSlot = slot;
    }
    if (matches != 1) return false;

    g_SwitchControllerSlot = foundSlot;
    return true;
}

bool WritePointer(void *address, void *value) {
    if (!address) return false;
    DWORD oldProtect = 0;
    if (!VirtualProtect(address, sizeof(void *), PAGE_READWRITE, &oldProtect)) return false;
    *reinterpret_cast<void **>(address) = value;
    DWORD restoredProtect = 0;
    VirtualProtect(address, sizeof(void *), oldProtect, &restoredProtect);
    return SafeMem_ReadPtr(address) == value;
}

bool CallSwitchControllerRaw(void *function, void *localPlayer, void *playerController) {
    using SwitchControllerFn = void(__fastcall *)(void *, void *);
    __try {
        reinterpret_cast<SwitchControllerFn>(function)(localPlayer, playerController);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void MoveLocalPlayerTo(UObject *localPlayer, UObject *fromController, UObject *toController) {
    void *vtable = ReadPtrAt(localPlayer, 0);
    if (!vtable) return;
    void *function = ReadPtrAt(vtable, static_cast<size_t>(g_SwitchControllerSlot) * sizeof(void *));
    if (!function) return;

    uint8_t code[kCodeProbeBytes] = {};
    if (!ReadBytes(function, code, sizeof(code)) || !WritesPlayerControllerToPlayer(code)) return;
    if (!CallSwitchControllerRaw(function, localPlayer, toController)) {
        LogLine("[TravelGuard] SwitchController faulted; controller attachment left unchanged.");
        return;
    }

    char buffer[1024] = {};
    std::snprintf(buffer, sizeof(buffer),
                  "[TravelGuard] Local player reattached to %s before seamless travel (%s had no game controller).",
                  EngineDiscovery::BuildFullName(*g_State, toController).c_str(),
                  fromController ? EngineDiscovery::BuildFullName(*g_State, fromController).c_str() : "no controller");
    LogLine(buffer);
}

UObject *SingleLocalPlayer() {
    if (!g_State || !g_State->GEngine) return nullptr;
    UObject *engine = static_cast<UObject *>(g_State->GEngine);
    UClass *engineClass = EngineDiscovery::GetObjectClass(*g_State, engine);
    if (!engineClass) return nullptr;

    const int32_t viewportOffset = ConsoleEnabler::FindPropertyOffset(*g_State, reinterpret_cast<UStruct *>(engineClass), "GameViewport");
    if (viewportOffset < 0) return nullptr;
    UObject *viewport = static_cast<UObject *>(ReadPtrAt(engine, static_cast<size_t>(viewportOffset)));
    if (!viewport) return nullptr;
    UClass *viewportClass = EngineDiscovery::GetObjectClass(*g_State, viewport);
    if (!viewportClass) return nullptr;

    const int32_t gameInstanceOffset = ConsoleEnabler::FindPropertyOffset(*g_State, reinterpret_cast<UStruct *>(viewportClass), "GameInstance");
    if (gameInstanceOffset < 0) return nullptr;
    UObject *gameInstance = static_cast<UObject *>(ReadPtrAt(viewport, static_cast<size_t>(gameInstanceOffset)));
    if (!gameInstance) return nullptr;
    UClass *gameInstanceClass = EngineDiscovery::GetObjectClass(*g_State, gameInstance);
    if (!gameInstanceClass) return nullptr;

    const int32_t localPlayersOffset = ConsoleEnabler::FindPropertyOffset(*g_State, reinterpret_cast<UStruct *>(gameInstanceClass), "LocalPlayers");
    if (localPlayersOffset < 0) return nullptr;
    uint8_t array[16] = {};
    if (!SafeMem_Read(reinterpret_cast<const uint8_t *>(gameInstance) + localPlayersOffset, array, sizeof(array))) {
        return nullptr;
    }
    void *data = *reinterpret_cast<void **>(array);
    const int32_t count = *reinterpret_cast<int32_t *>(array + 8);
    if (!data || count != 1) return nullptr;
    return static_cast<UObject *>(ReadPtrAt(data, 0));
}

UObject *WorldForObject(UObject *object) {
    if (!g_State || !object) return nullptr;
    void *outer = ReadPtrAt(object, static_cast<size_t>(g_State->UObject_OuterOff));
    for (int depth = 0; depth < 4 && outer; ++depth) {
        auto *candidate = static_cast<UObject *>(outer);
        UClass *candidateClass = EngineDiscovery::GetObjectClass(*g_State, candidate);
        if (candidateClass && g_State->UWorldClass &&
            EngineDiscovery::IsStructA(*g_State, reinterpret_cast<UStruct *>(candidateClass),
                                       reinterpret_cast<UStruct *>(g_State->UWorldClass))) {
            return candidate;
        }
        outer = ReadPtrAt(outer, static_cast<size_t>(g_State->UObject_OuterOff));
    }
    return nullptr;
}

UClass *CurrentGamePlayerControllerClass(UObject *playerController) {
    UObject *world = WorldForObject(playerController);
    if (!world) return nullptr;
    UObject *gameMode = static_cast<UObject *>(ReadPtrAt(world, static_cast<size_t>(g_AuthorityGameModeOffset)));
    if (!gameMode) return nullptr;
    UClass *gameModeClass = EngineDiscovery::GetObjectClass(*g_State, gameMode);
    if (!gameModeClass) return nullptr;

    const int32_t controllerClassOffset = ConsoleEnabler::FindPropertyOffset(*g_State, reinterpret_cast<UStruct *>(gameModeClass), "PlayerControllerClass");
    if (controllerClassOffset < 0) return nullptr;
    UClass *controllerClass = static_cast<UClass *>(ReadPtrAt(gameMode, static_cast<size_t>(controllerClassOffset)));
    if (!controllerClass || !ConsoleEnabler::IsClassObject(*g_State, reinterpret_cast<UObject *>(controllerClass))) {
        return nullptr;
    }
    return controllerClass;
}

UObject *DetachedGamePlayerController(UClass *gameControllerClass) {
    static std::vector<void *> actors(32768, nullptr);
    const int32_t count = EngineDiscovery::GetWorldActors(*g_State, actors.data(), static_cast<int32_t>(actors.size()));

    UObject *found = nullptr;
    for (int32_t i = 0; i < count; ++i) {
        UObject *actor = static_cast<UObject *>(actors[static_cast<size_t>(i)]);
        if (!actor) continue;
        UClass *actorClass = EngineDiscovery::GetObjectClass(*g_State, actor);
        if (!EngineDiscovery::IsStructA(*g_State, reinterpret_cast<UStruct *>(actorClass),
                                        reinterpret_cast<UStruct *>(gameControllerClass))) {
            continue;
        }
        if (ReadPtrAt(actor, static_cast<size_t>(g_PlayerOffset))) continue;
        if (!ReadPtrAt(actor, static_cast<size_t>(g_PlayerStateOffset))) continue;
        if (found) return nullptr;
        found = actor;
    }
    return found;
}

void RepairAttachment(UObject *playerController) {
    if (t_Repairing) return;
    t_Repairing = true;

    UClass *gameControllerClass = CurrentGamePlayerControllerClass(playerController);
    if (!gameControllerClass) {
        t_Repairing = false;
        return;
    }

    UClass *controllerClass = EngineDiscovery::GetObjectClass(*g_State, playerController);
    const bool isGameController = EngineDiscovery::IsStructA(
        *g_State, reinterpret_cast<UStruct *>(controllerClass), reinterpret_cast<UStruct *>(gameControllerClass));

    UObject *player = static_cast<UObject *>(ReadPtrAt(playerController, static_cast<size_t>(g_PlayerOffset)));
    if (!player) {
        if (!isGameController || !ReadPtrAt(playerController, static_cast<size_t>(g_PlayerStateOffset))) {
            t_Repairing = false;
            return;
        }
        UObject *localPlayer = SingleLocalPlayer();
        if (!localPlayer) {
            t_Repairing = false;
            return;
        }
        UObject *attachedController = static_cast<UObject *>(ReadPtrAt(localPlayer, static_cast<size_t>(g_PlayerControllerOffset)));
        if (!attachedController || attachedController == playerController) {
            t_Repairing = false;
            return;
        }
        UClass *attachedClass = EngineDiscovery::GetObjectClass(*g_State, attachedController);
        if (EngineDiscovery::IsStructA(*g_State, reinterpret_cast<UStruct *>(attachedClass),
                                       reinterpret_cast<UStruct *>(gameControllerClass))) {
            t_Repairing = false;
            return;
        }
        MoveLocalPlayerTo(localPlayer, attachedController, playerController);
        t_Repairing = false;
        return;
    }

    if (isGameController) {
        t_Repairing = false;
        return;
    }

    UObject *localPlayer = SingleLocalPlayer();
    if (!localPlayer) {
        t_Repairing = false;
        return;
    }
    UObject *attachedController = static_cast<UObject *>(ReadPtrAt(localPlayer, static_cast<size_t>(g_PlayerControllerOffset)));
    if (attachedController != playerController) {
        t_Repairing = false;
        return;
    }
    UObject *detached = DetachedGamePlayerController(gameControllerClass);
    if (detached && detached != playerController) {
        MoveLocalPlayerTo(localPlayer, playerController, detached);
    }
    t_Repairing = false;
}

bool ShouldParticipateHook(void *playerController) {
    if (!g_Ready || !g_State || !playerController) return true;

    UObject *controller = static_cast<UObject *>(playerController);
    RepairAttachment(controller);

    const uint8_t *playerField = reinterpret_cast<const uint8_t *>(controller) + g_PlayerOffset;
    if (!SafeMem_IsReadable(playerField, sizeof(void *))) return true;
    return SafeMem_ReadPtr(playerField) != nullptr;
}

struct PatchContext {
    int patched = 0;
};

bool PatchClassCallback(UObject *object, int32_t, void *userdata) {
    if (!ConsoleEnabler::IsClassObject(*g_State, object)) return true;

    if (g_State->UObject_FlagsOff >= 0) {
        uint32_t flags = 0;
        if (SafeMem_Read(reinterpret_cast<const uint8_t *>(object) + g_State->UObject_FlagsOff,
                         &flags, sizeof(flags)) &&
            (flags & kRfObjectTemplateMask) != 0) {
            return true;
        }
    }

    auto *candidateClass = reinterpret_cast<UClass *>(object);
    if (!EngineDiscovery::IsStructA(*g_State, reinterpret_cast<UStruct *>(candidateClass),
                                    reinterpret_cast<UStruct *>(g_PlayerControllerClass))) {
        return true;
    }

    UObject *classDefaultObject = EngineDiscovery::FindClassDefaultObject(*g_State, candidateClass);
    void *vtable = ReadPtrAt(classDefaultObject, 0);
    if (!vtable) return true;

    void **slot = reinterpret_cast<void **>(reinterpret_cast<uint8_t *>(vtable) +
                                            static_cast<size_t>(g_ParticipationSlot) * sizeof(void *));
    if (!SafeMem_IsReadable(slot, sizeof(void *)) || SafeMem_ReadPtr(slot) != g_ParticipationStub) {
        return true;
    }

    void *hook = reinterpret_cast<void *>(&ShouldParticipateHook);
    if (!WritePointer(slot, hook)) return true;

    ++reinterpret_cast<PatchContext *>(userdata)->patched;
    ++g_TotalPatched;

    char buffer[512] = {};
    std::snprintf(buffer, sizeof(buffer), "[TravelGuard] Patched participation slot on %s",
                  EngineDiscovery::GetObjectNameStr(*g_State, object).c_str());
    LogLine(buffer);
    return true;
}

}

bool Install(EngineState &state) {
    if (!state.bInitialized) return false;
    g_State = &state;

    if (state.UEMajor > 0 &&
        (state.UEMajor < 5 || (state.UEMajor == 5 && state.UEMinor < 1))) {
        if (!g_LoggedDiscoveryFailure) {
            g_LoggedDiscoveryFailure = true;
            LogLine("[TravelGuard] PlayerController participation is a UE5.1+ engine virtual; this build has no participation filter, so nothing is patched.");
        }
        g_Ready = false;
        return false;
    }

    if (!ResolveInterfaces(state)) {
        g_Ready = false;
        return false;
    }

    if (g_ParticipationSlot < 0 && !DiscoverParticipationSlot()) {
        if (!g_LoggedDiscoveryFailure) {
            g_LoggedDiscoveryFailure = true;
            LogLine("[TravelGuard] Could not identify the PlayerController participation override; guard disabled.");
        }
        g_Ready = false;
        return false;
    }
    if (g_SwitchControllerSlot < 0 && !DiscoverSwitchControllerSlot()) {
        if (!g_LoggedDiscoveryFailure) {
            g_LoggedDiscoveryFailure = true;
            LogLine("[TravelGuard] Could not identify UPlayer::SwitchController; guard disabled.");
        }
        g_Ready = false;
        return false;
    }

    g_Ready = true;

    PatchContext context;
    EngineDiscovery::ForEachObject(state, &PatchClassCallback, &context);
    if (context.patched > 0) {
        char buffer[320] = {};
        std::snprintf(buffer, sizeof(buffer),
                      "[TravelGuard] Participation slot %d (0x%X), SwitchController slot %d, Player+0x%X, "
                      "PlayerState+0x%X; patched %d class vtable(s), %d total.",
                      g_ParticipationSlot, g_ParticipationSlot * 8, g_SwitchControllerSlot,
                      g_PlayerOffset, g_PlayerStateOffset, context.patched, g_TotalPatched);
        LogLine(buffer);
    }
    return true;
}

}
