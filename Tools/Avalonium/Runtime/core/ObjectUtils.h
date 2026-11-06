#pragma once
#include "Types.h"
#include <string>

namespace EngineDiscovery {

UClass *GetObjectClass(const EngineState &state, UObject *obj);
UObject *GetObjectOuter(const EngineState &state, UObject *obj);
UStruct *GetSuperStruct(const EngineState &state, UStruct *ustruct);

void *GetChildrenHead(const EngineState &state, UStruct *ustruct);
void *GetFieldNext(const EngineState &state, void *field);
std::string GetFieldName(const EngineState &state, void *field);

void *GetChildPropsHead(const EngineState &state, UStruct *ustruct);
void *GetFFieldNext(const EngineState &state, void *ffield);
std::string GetFFieldName(const EngineState &state, void *ffield);
std::string GetFFieldTypeName(const EngineState &state, void *ffield);

void RecordPropertyTypeClass(const std::string &typeName, void *classInstance);
void *GetRecordedPropertyTypeClass(const std::string &typeName);

int32_t GetPropertyAlignment(const EngineState &state, void *property,
                             int32_t valueSize);void ApplyFFieldLayoutFixups(EngineState& state);
void DiscoverFPropertyOffsetInternal(EngineState& state);

bool ResolveUStructPropertiesSizeOffset(EngineState& state);

int32_t UStructPropertiesSizeOffset(const EngineState& state);

bool ResolveUStructMinAlignmentOffset(EngineState& state);

bool ResolveUClassDefaultObjectOffset(EngineState& state);

}
