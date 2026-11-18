#pragma once
#include "Types.h"

namespace EngineDiscovery {

struct DataTableRowMap
{
    uint8_t* Data = nullptr;
    int32_t ArrayNum = 0;
    int32_t MapOffset = -1;
    int32_t Stride = 0;
    UStruct* RowStruct = nullptr;
    bool Validated = false;
};

bool ResolveDataTableRowMap(EngineState &state, void *dataTable,
                            int32_t rowStructOffsetHint, DataTableRowMap &out);

UStruct *GetDataTableRowStruct(EngineState &state, void *dataTable,
                               int32_t rowStructOffsetHint);

int32_t ResolveRowMapStride(EngineState &state, const void *mapData,
                            int32_t arrayNum, void *rowStruct);

}
