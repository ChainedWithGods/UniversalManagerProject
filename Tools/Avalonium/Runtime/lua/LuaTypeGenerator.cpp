#include "LuaTypeGenerator.h"
#include "LuaUObject.h"
#include "LuaTypes.h"
#include "../core/EngineDiscovery.h"
#include "../core/Types.h"
#include "../utils/SafeMemory.h"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <algorithm>

extern EngineState g_State;

namespace LuaEngine {

static std::string MapPropertyTypeToLua(EngineState& state, void* prop) {
    if (!prop) return "any";
    std::string pClass = EngineDiscovery::GetObjectNameStr(state, static_cast<UObject*>(EngineDiscovery::GetObjectClass(state, static_cast<UObject*>(prop))));

    if (pClass == "BoolProperty") return "boolean";
    if (pClass == "IntProperty" || pClass == "Int64Property" || pClass == "ByteProperty" || pClass == "UInt32Property") return "integer";
    if (pClass == "FloatProperty" || pClass == "DoubleProperty") return "number";
    if (pClass == "StrProperty" || pClass == "NameProperty" || pClass == "TextProperty") return "string";
    if (pClass == "StructProperty") return "any";
    if (pClass == "ArrayProperty") return "table";
    if (pClass == "MapProperty" || pClass == "SetProperty") return "table";
    if (pClass == "ObjectProperty" || pClass == "ClassProperty") return "UObject";
    return "any";
}

bool LuaTypeGenerator::GenerateLuaTypes(const std::string& outputDir) {
    if (!g_State.GUObjectArray) {
        EngineDiscovery::WriteLogLine("[LuaTypeGen] GUObjectArray not available.");
        return false;
    }

    std::string gameName = (g_State.GameName[0] ? g_State.GameName : "Default");
    std::string targetDir = outputDir.empty() ? ("Mods/shared/types/" + gameName) : outputDir;

    std::error_code ec;
    std::filesystem::create_directories(targetDir, ec);

    std::string outFile = targetDir + "/AvaloniumTypes.lua";
    std::filesystem::path absOutFile = std::filesystem::absolute(outFile);

    char startMsg[512];
    snprintf(startMsg, sizeof(startMsg), "[LuaTypeGen] Generating EmmyLua type definitions to: %s", absOutFile.string().c_str());
    EngineDiscovery::WriteLogLine(startMsg);

    std::ofstream out(outFile);
    if (!out.is_open()) {
        char msg[512];
        snprintf(msg, sizeof(msg), "[LuaTypeGen] Failed to open output file: %s", absOutFile.string().c_str());
        EngineDiscovery::WriteLogLine(msg);
        return false;
    }

    out << "---@meta\n";
    out << "→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←\n";
    out << " Avalonium Auto-Generated Lua Type Definitions (EmmyLua / LuaLS compatible)\n";
    out << "→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←\n";

    out << "---@class FVector\n";
    out << "---@field X number\n";
    out << "---@field Y number\n";
    out << "---@field Z number\n";
    out << "---@field Length fun(self: FVector): number\n";
    out << "---@field Length2D fun(self: FVector): number\n";
    out << "---@field LengthSquared fun(self: FVector): number\n";
    out << "---@field Distance fun(self: FVector, other: FVector): number\n";
    out << "---@field Dot fun(self: FVector, other: FVector): number\n";
    out << "---@field Cross fun(self: FVector, other: FVector): FVector\n";
    out << "---@field Normalize fun(self: FVector): FVector\n";
    out << "---@field ToRotator fun(self: FVector): FRotator\n";
    out << "---@field ToTable fun(self: FVector): {X: number, Y: number, Z: number}\n";
    out << "---@field Clone fun(self: FVector): FVector\n";
    out << "---@operator add(FVector): FVector\n";
    out << "---@operator sub(FVector): FVector\n";
    out << "---@operator mul(number): FVector\n";
    out << "---@operator div(number): FVector\n";
    out << "---@operator unm: FVector\n";
    out << "local FVector = {}\n\n";

    out << "---@class FRotator\n";
    out << "---@field Pitch number\n";
    out << "---@field Yaw number\n";
    out << "---@field Roll number\n";
    out << "---@field GetForwardVector fun(self: FRotator): FVector\n";
    out << "---@field GetRightVector fun(self: FRotator): FVector\n";
    out << "---@field GetUpVector fun(self: FRotator): FVector\n";
    out << "---@field Normalize fun(self: FRotator): FRotator\n";
    out << "---@field Clamp fun(self: FRotator): FRotator\n";
    out << "---@field ToTable fun(self: FRotator): {Pitch: number, Yaw: number, Roll: number}\n";
    out << "---@field Clone fun(self: FRotator): FRotator\n";
    out << "---@operator add(FRotator): FRotator\n";
    out << "---@operator sub(FRotator): FRotator\n";
    out << "local FRotator = {}\n\n";

    out << "---@class FTransform\n";
    out << "---@field Rotation FRotator\n";
    out << "---@field Translation FVector\n";
    out << "---@field Scale3D FVector\n";
    out << "---@field TransformPosition fun(self: FTransform, point: FVector): FVector\n";
    out << "---@field ToTable fun(self: FTransform): table\n";
    out << "---@field Clone fun(self: FTransform): FTransform\n";
    out << "local FTransform = {}\n\n";

    out << "---@class FLinearColor\n";
    out << "---@field R number\n";
    out << "---@field G number\n";
    out << "---@field B number\n";
    out << "---@field A number\n";
    out << "---@field ToHex fun(self: FLinearColor): string\n";
    out << "---@field ToTable fun(self: FLinearColor): {R: number, G: number, B: number, A: number}\n";
    out << "---@field Clone fun(self: FLinearColor): FLinearColor\n";
    out << "local FLinearColor = {}\n\n";

    out << "---@class UObject\n";
    out << "---@field GetName fun(self: UObject): string\n";
    out << "---@field GetFullName fun(self: UObject): string\n";
    out << "---@field GetPathName fun(self: UObject): string\n";
    out << "---@field GetFName fun(self: UObject): string\n";
    out << "---@field GetClass fun(self: UObject): UObject\n";
    out << "---@field GetOuter fun(self: UObject): UObject\n";
    out << "---@field GetPackage fun(self: UObject): UObject\n";
    out << "---@field GetAddress fun(self: UObject): integer\n";
    out << "---@field GetAddressString fun(self: UObject): string\n";
    out << "---@field IsA fun(self: UObject, className: string): boolean\n";
    out << "---@field IsValid fun(self: UObject): boolean\n";
    out << "---@field Cast fun(self: UObject, className: string): UObject|nil\n";
    out << "---@field GetProperty fun(self: UObject, name: string): any\n";
    out << "---@field GetPropertyValue fun(self: UObject, name: string): any\n";
    out << "---@field SetProperty fun(self: UObject, name: string, value: any): boolean\n";
    out << "---@field SetPropertyValue fun(self: UObject, name: string, value: any): boolean\n";
    out << "---@field GetProperties fun(self: UObject): table<string, string>\n";
    out << "---@field FindProperty fun(self: UObject, searchStr: string): string|nil\n";
    out << "---@field GetFunctions fun(self: UObject): string[]\n";
    out << "---@field HasFunction fun(self: UObject, name: string): boolean\n";
    out << "---@field Call fun(self: UObject, funcName: string, ...: any): any\n";
    out << "---@field CallFunction fun(self: UObject, funcName: string, ...: any): any\n";
    out << "---@field GetComponent fun(self: UObject, nameOrClass: string): UObject|nil\n";
    out << "---@field GetComponents fun(self: UObject): UObject[]\n";
    out << "---@field GetComponentsByClass fun(self: UObject, className: string): UObject[]\n";
    out << "---@field GetCDO fun(self: UObject): UObject|nil\n";
    out << "---@field GetClassDefaultObject fun(self: UObject): UObject|nil\n";
    out << "---@field GetSuperClass fun(self: UObject): UObject|nil\n";
    out << "---@field GetChildClasses fun(self: UObject): UObject[]\n";
    out << "---@field IsChildOf fun(self: UObject, className: string): boolean\n";
    out << "---@field GetWorld fun(self: UObject): UObject|nil\n";
    out << "---@field DumpProperties fun(self: UObject): table<string, {Type: string, Offset: integer, Size: integer, Value: any}>\n";
    out << "---@field DumpFunctions fun(self: UObject): table<string, {Flags: string, RawFlags: integer}>\n";
    out << "---@field GetLocation fun(self: UObject): FVector\n";
    out << "---@field SetLocation fun(self: UObject, loc: FVector): boolean\n";
    out << "---@field GetRotation fun(self: UObject): FRotator\n";
    out << "---@field SetRotation fun(self: UObject, rot: FRotator): boolean\n";
    out << "---@field GetScale fun(self: UObject): FVector\n";
    out << "---@field SetScale fun(self: UObject, scale: FVector): boolean\n";
    out << "---@field GetTransform fun(self: UObject): FTransform\n";
    out << "---@field SetTransform fun(self: UObject, transform: FTransform): boolean\n";
    out << "---@field Teleport fun(self: UObject, loc: FVector, rot?: FRotator): boolean\n";
    out << "---@field GetVelocity fun(self: UObject): FVector\n";
    out << "---@field GetDistanceTo fun(self: UObject, other: UObject|FVector): number\n";
    out << "---@field GetForwardVector fun(self: UObject): FVector\n";
    out << "---@field GetRightVector fun(self: UObject): FVector\n";
    out << "---@field GetUpVector fun(self: UObject): FVector\n";
    out << "---@field AddActorWorldOffset fun(self: UObject, delta: FVector): boolean\n";
    out << "---@field AddActorLocalOffset fun(self: UObject, delta: FVector): boolean\n";
    out << "---@field AddActorWorldRotation fun(self: UObject, delta: FRotator): boolean\n";
    out << "---@field AddActorLocalRotation fun(self: UObject, delta: FRotator): boolean\n";
    out << "---@field GetRootComponent fun(self: UObject): UObject|nil\n";
    out << "---@field AttachToActor fun(self: UObject, parent: UObject): boolean\n";
    out << "---@field AttachToComponent fun(self: UObject, parentComp: UObject): boolean\n";
    out << "---@field DetachFromActor fun(self: UObject): boolean\n";
    out << "---@field GetAttachedActors fun(self: UObject): UObject[]\n";
    out << "---@field GetActorBounds fun(self: UObject, bOnlyColliding?: boolean): FVector, FVector\n";
    out << "---@field DestroyActor fun(self: UObject): boolean\n";
    out << "---@field SetActorHiddenInGame fun(self: UObject, bHidden: boolean): boolean\n";
    out << "---@field IsHidden fun(self: UObject): boolean\n";
    out << "---@field SetActorEnableCollision fun(self: UObject, bEnable: boolean): boolean\n";
    out << "local UObject = {}\n\n";

    out << "---@class HookParams\n";
    out << "---@field Get fun(self: HookParams, propName: string): any\n";
    out << "---@field Set fun(self: HookParams, propName: string, value: any): boolean\n";
    out << "---@field Cancel fun(self: HookParams): void\n";
    out << "---@field IsCancelled fun(self: HookParams): boolean\n";
    out << "---@field GetSelf fun(self: HookParams): UObject|nil\n";
    out << "---@field GetFunction fun(self: HookParams): UObject|nil\n";
    out << "---@field GetReturnValue fun(self: HookParams): any\n";
    out << "---@field SetReturnValue fun(self: HookParams, value: any): boolean\n";
    out << "---@field Dump fun(self: HookParams): table<string, {Type: string, Offset: integer, IsOut: boolean, IsReturn: boolean, Value: any}>\n";
    out << "local HookParams = {}\n\n";

    out << "---@class File\n";
    out << "---@field Read fun(path: string): string|nil\n";
    out << "---@field Write fun(path: string, content: string): boolean\n";
    out << "---@field Append fun(path: string, content: string): boolean\n";
    out << "---@field Exists fun(path: string): boolean\n";
    out << "---@field List fun(dirPath: string): string[]\n";
    out << "---@field CreateDir fun(dirPath: string): boolean\n";
    out << "---@field Delete fun(path: string): boolean\n";
    out << "File = {}\n\n";

    out << "---@class JSON\n";
    out << "---@field Parse fun(str: string): any\n";
    out << "---@field Stringify fun(val: any, pretty?: boolean): string\n";
    out << "JSON = {}\n\n";

    out << "---@class Math\n";
    out << "---@field Lerp fun(a: number, b: number, alpha: number): number\n";
    out << "---@field Clamp fun(value: number, min: number, max: number): number\n";
    out << "---@field VInterpTo fun(current: FVector, target: FVector, deltaTime: number, interpSpeed: number): FVector\n";
    out << "---@field RInterpTo fun(current: FRotator, target: FRotator, deltaTime: number, interpSpeed: number): FRotator\n";
    out << "---@field FindLookAtRotation fun(start: FVector, target: FVector): FRotator\n";
    out << "Math = {}\n\n";

    out << "---@class MemoryUsage\n";
    out << "---@field [1] number Physical memory used in MB (Working Set)\n";
    out << "---@field [2] number Peak physical memory used in MB (Peak Working Set)\n";
    out << "---@field [3] number Available physical RAM in MB\n";
    out << "---@field usedMB number Physical memory used in MB\n";
    out << "---@field peakMB number Peak physical memory used in MB\n";
    out << "---@field availMB number Available physical RAM in MB\n";
    out << "---@field privateMB number Private committed memory in MB\n";
    out << "---@field totalMB number Total physical system RAM in MB\n\n";

    out << "---@type fun(): number Returns current frame time in milliseconds (e.g. 16.6ms)\n";
    out << "GetFrameTime = nil\n\n";

    out << "---@type fun(): number Returns frames per second (e.g. 60.0)\n";
    out << "GetFPS = nil\n\n";

    out << "---@type fun(): number Returns frame delta in seconds (e.g. 0.0166s)\n";
    out << "GetDeltaSeconds = nil\n\n";

    out << "---@type fun(): MemoryUsage Returns process and system memory metrics in MB\n";
    out << "GetMemoryUsage = nil\n\n";

    out << "---@type fun(fullPurge?: boolean): boolean Triggers full engine garbage collection\n";
    out << "ForceGarbageCollection = nil\n\n";

    out << "---@type fun(fullPurge?: boolean): boolean Triggers full engine garbage collection (alias)\n";
    out << "CollectGarbage = nil\n\n";

    std::unordered_set<std::string> emittedClasses;
    int count = EngineDiscovery::GetObjectCount(g_State);

    for (int i = 0; i < count; ++i) {
        void* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
        if (!obj) continue;

        UClass* clsObj = EngineDiscovery::GetObjectClass(g_State, static_cast<UObject*>(obj));
        if (!clsObj) continue;

        std::string clsName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(clsObj));
        if (clsName != "Class" && clsName != "BlueprintGeneratedClass") continue;

        std::string objName = EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(obj));
        if (objName.empty() || emittedClasses.find(objName) != emittedClasses.end()) continue;
        emittedClasses.insert(objName);

        UStruct* superStruct = EngineDiscovery::GetSuperStruct(g_State, static_cast<UStruct*>(obj));
        std::string superName = superStruct ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(superStruct)) : "UObject";

        out << "---@class " << objName << " : " << superName << "\n";

        if (g_State.UStruct_ChildPropsOff >= 0) {
            void* childField = EngineDiscovery::GetChildPropsHead(g_State, static_cast<UStruct*>(obj));
            while (childField) {
                std::string fieldClass = EngineDiscovery::GetFFieldTypeName(g_State, childField);
                std::string fieldName = EngineDiscovery::GetFFieldName(g_State, childField);

                if (fieldClass.find("Function") != std::string::npos) {
                    out << "---@field " << fieldName << " fun(self: " << objName << ", ...: any): any\n";
                } else if (fieldClass.find("Property") != std::string::npos) {
                    std::string luaType = MapPropertyTypeToLua(g_State, childField);
                    out << "---@field " << fieldName << " " << luaType << "\n";
                }

                childField = EngineDiscovery::GetFFieldNext(g_State, childField);
            }
        }

        out << "local " << objName << " = {}\n\n";
    }

    out.close();
    char msg[512];
    snprintf(msg, sizeof(msg), "[LuaTypeGen] Successfully generated EmmyLua type definitions to: %s (%zu classes)",
        absOutFile.string().c_str(), emittedClasses.size());
    EngineDiscovery::WriteLogLine(msg);
    return true;
}

bool LuaTypeGenerator::DumpAllObjects(const std::string& filePath) {
    if (!g_State.GUObjectArray) {
        EngineDiscovery::WriteLogLine("[ObjectDumper] GUObjectArray not available.");
        return false;
    }

    std::filesystem::path absFilePath = std::filesystem::absolute(filePath);
    std::ofstream out(filePath);
    if (!out.is_open()) {
        char msg[512];
        snprintf(msg, sizeof(msg), "[ObjectDumper] Failed to open dump file: %s", absFilePath.string().c_str());
        EngineDiscovery::WriteLogLine(msg);
        return false;
    }

    int count = EngineDiscovery::GetObjectCount(g_State);
    out << "→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←\n";
    out << " Avalonium Object Dump (Total GUObjectArray count: " << count << ")\n";
    out << "→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←→←\n";

    for (int i = 0; i < count; ++i) {
        void* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
        if (!obj) continue;

        UObject* uObj = static_cast<UObject*>(obj);
        std::string fullName = EngineDiscovery::BuildFullName(g_State, uObj);
        UClass* cls = EngineDiscovery::GetObjectClass(g_State, uObj);
        std::string clsName = cls ? EngineDiscovery::GetObjectNameStr(g_State, static_cast<UObject*>(cls)) : "Unknown";

        out << "[" << i << "] [0x" << std::hex << reinterpret_cast<uintptr_t>(obj) << std::dec << "] "
            << fullName << " (Class: " << clsName << ")\n";
    }

    out.close();
    char msg[512];
    snprintf(msg, sizeof(msg), "[ObjectDumper] Successfully dumped %d objects to %s", count, absFilePath.string().c_str());
    EngineDiscovery::WriteLogLine(msg);
    return true;
}

int LuaTypeGenerator::Lua_GenerateLuaTypes(lua_State* L) {
    std::string gameName = (g_State.GameName[0] ? g_State.GameName : "Default");
    std::string outDir = "Mods/shared/types/" + gameName;
    if (lua_gettop(L) >= 1 && lua_isstring(L, 1)) {
        outDir = lua_tostring(L, 1);
    }
    bool res = GenerateLuaTypes(outDir);
    lua_pushboolean(L, res ? 1 : 0);
    return 1;
}

int LuaTypeGenerator::Lua_DumpAllObjects(lua_State* L) {
    std::string outPath = "Avalonium_ObjectDump.txt";
    if (lua_gettop(L) >= 1 && lua_isstring(L, 1)) {
        outPath = lua_tostring(L, 1);
    }
    bool res = DumpAllObjects(outPath);
    lua_pushboolean(L, res ? 1 : 0);
    return 1;
}

int LuaTypeGenerator::Lua_ForEachUObject(lua_State* L) {
    if (!lua_isfunction(L, 1)) {
        return luaL_error(L, "ForEachUObject requires a callback function");
    }

    if (!g_State.GUObjectArray) return 0;

    int count = EngineDiscovery::GetObjectCount(g_State);
    for (int i = 0; i < count; ++i) {
        void* obj = EngineDiscovery::GetObjectByIndex(g_State, i);
        if (!obj) continue;

        lua_pushvalue(L, 1);
        PushUObject(L, static_cast<UObject*>(obj));
        lua_pushinteger(L, i);

        if (lua_pcall(L, 2, 1, 0) == LUA_OK) {
            if (lua_isboolean(L, -1) && !lua_toboolean(L, -1)) {
                lua_pop(L, 1);
                break;
            }
            lua_pop(L, 1);
        }
    }
    return 0;
}

}
