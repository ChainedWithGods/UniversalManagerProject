#pragma once
#include "EngineDiscovery.h"
#include <string>

namespace EngineDiscovery {

bool IsStructA(const EngineState &state, UStruct *type, UStruct *want);

bool ResolveProcessEventImplementations(EngineState &state, int slotIdx,
                                        void **uobjectImpl, void **aactorImpl,
                                        bool *distinct);

int FindDirectBranchTo(void *from, void *to, int window = 0x400);

int FindFuncNativeFlagsOffset(void *fn, int window = 0x800);

struct ProcessEventSlotLaw {
  int slot = -1;
  void *uobjectImpl = nullptr;
  void *aactorImpl = nullptr;
  int overrideBranchOffset = -1;
  int flagsOffset = -1;
  int objectReads = 0, objectAgree = 0;
  int actorReads = 0, actorAgree = 0;
};

bool FindProcessEventSlotByEngineLaw(EngineState &state, int slotLo, int slotHi,
                                     ProcessEventSlotLaw *out);

void ClearInheritanceCache();

bool FindGetFullName(EngineState &state);

bool FindGEngine(EngineState &state);

bool FindStaticConstructObject(EngineState &state);

bool FindConsoleManagerHeuristic(EngineState &state);

bool FindConsoleManager(EngineState &state);

bool FindEngineFunctions(EngineState &state, bool skipConsoleManager);

bool FindCoreClasses(EngineState &state);

bool FindProcessEvent(EngineState &state);

bool FindProcessInternal(EngineState &state);
bool FindProcessLocalScriptFunction(EngineState &state);

void *FindFunctionByName(const EngineState &state, UStruct *cls,
                         const char *funcName);

bool CallProcessEvent(const EngineState &state, void *object, void *ufunction,
                      void *params);

bool IsLoaderDispatchedCall();

}
