cmake_minimum_required(VERSION 3.20)
project(Avalonium VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(HELPERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../Helpers)
set(IMGUI_HOOK_DIR ${HELPERS_DIR}/Universal-Dear-ImGui-Hook)
set(MINHOOK_DIR ${HELPERS_DIR}/minhook)


include(FetchContent)
FetchContent_Declare(lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(lua)
set(LUA_DIR ${lua_SOURCE_DIR})


if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8" OR
   "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" STREQUAL "x64" OR
   "${CMAKE_GENERATOR_PLATFORM}" STREQUAL "x64")
    set(MINHOOK_HDE_SOURCE ${MINHOOK_DIR}/src/hde/hde64.c)
else()
    set(MINHOOK_HDE_SOURCE ${MINHOOK_DIR}/src/hde/hde32.c)
endif()

set(SOURCES
    core/dllmain.cpp
    core/Logger.cpp
    core/LogSession.cpp
    core/PatternScanner.cpp
    core/ObjectArray.cpp
    core/NameResolution.cpp
    core/ObjectUtils.cpp
    core/WorldDiscovery.cpp
    core/EngineDetection.cpp
    core/AllocatorSlotScan.cpp
    core/EngineDiscovery.cpp
    core/GNativesResolver.cpp
    core/EncryptedCache.cpp
    core/OffsetDiscovery.cpp
    core/FunctionFinder.cpp
    core/ObjectIndex.cpp
    core/BlueprintDiscovery.cpp
    core/RuntimeConfig.cpp
    lua/LuaManager.cpp
    lua/LuaTypes.cpp
    lua/LuaUObject.cpp
    lua/LuaHookManager.cpp
    lua/LuaTypeGenerator.cpp
    lua/LuaCompatApi.cpp
    lua/LuaCompatParity.cpp
    lua/LuaCompatSurface.cpp
    lua/BuiltinMods/BuiltinMods.cpp
    lua/BuiltinMods/UniversalHUD.cpp
    lua/BuiltinMods/UniversalOverlay.cpp

    cppmods/CppModManager.cpp
    cppmods/CppModValue.cpp
    cppmods/CppModReflection.cpp
    cppmods/CppModHooks.cpp
    cppmods/CppModWidgets.cpp
    cppmods/CppModContainers.cpp
    cppmods/CppModNative.cpp
    cppmods/CppModImGui.cpp
    cppmods/CppModApi.cpp

    console/ConsoleHelpers.cpp
    console/ConsoleEnabler.cpp
    console/ConsoleDumpers.cpp
    console/ConsoleVariables.cpp
    console/SeamlessTravelGuard.cpp
    
    api/UEInternalAPI.cpp
    api/IOCPServer.cpp
    api/IPCEvents.cpp
    api/IPCJobs.cpp
    api/IPCQuery.cpp
    
    overlay/OverlayDragMe.cpp
    overlay/OverlayHelpers.cpp
    overlay/OverlayWorldExplorer.cpp
    overlay/OverlayInspector.cpp
    overlay/OverlayLiveEditor.cpp
    overlay/OverlayGlobalSearch.cpp
    overlay/OverlayGlobalsView.cpp
    overlay/OverlayMenu.cpp
    overlay/LuaOverlay.cpp
    overlay/SystemMetrics.cpp
    
    usmap/USMapGenerator.cpp
    
    crypto/PakVerifier.cpp
    crypto/AESKeyFinder.cpp
    
    sdk/SDKGenerator.cpp
    sdk/SDKGenerator_Common.cpp
    sdk/SDKGenerator_TypeSystem.cpp
    sdk/SDKGenerator_Reflection.cpp
    sdk/SDKGenerator_CodeEmit.cpp
    sdk/SDKGenerator_Containers.cpp
    sdk/SDKGenerator_Bitfields.cpp
    sdk/SDKGenerator_LayoutRules.cpp
    sdk/SDKGenerator_Predefined.cpp
    sdk/SDKGenerator_Cycles.cpp
    sdk/SDKGenerator_Assertions.cpp
    sdk/SDKValidation.cpp
    
    utils/InlineHook.cpp
    utils/SafeMemory.c
    utils/version.rc
    
    ${LUA_DIR}/lapi.c
    ${LUA_DIR}/lauxlib.c
    ${LUA_DIR}/lbaselib.c
    ${LUA_DIR}/lcode.c
    ${LUA_DIR}/lcorolib.c
    ${LUA_DIR}/lctype.c
    ${LUA_DIR}/ldblib.c
    ${LUA_DIR}/ldebug.c
    ${LUA_DIR}/ldo.c
    ${LUA_DIR}/ldump.c
    ${LUA_DIR}/lfunc.c
    ${LUA_DIR}/lgc.c
    ${LUA_DIR}/linit.c
    ${LUA_DIR}/liolib.c
    ${LUA_DIR}/llex.c
    ${LUA_DIR}/lmathlib.c
    ${LUA_DIR}/lmem.c
    ${LUA_DIR}/loadlib.c
    ${LUA_DIR}/lobject.c
    ${LUA_DIR}/lopcodes.c
    ${LUA_DIR}/loslib.c
    ${LUA_DIR}/lparser.c
    ${LUA_DIR}/lstate.c
    ${LUA_DIR}/lstring.c
    ${LUA_DIR}/lstrlib.c
    ${LUA_DIR}/ltable.c
    ${LUA_DIR}/ltablib.c
    ${LUA_DIR}/ltm.c
    ${LUA_DIR}/lundump.c
    ${LUA_DIR}/lutf8lib.c
    ${LUA_DIR}/lvm.c
    ${LUA_DIR}/lzio.c
    
    ${IMGUI_HOOK_DIR}/globals.cpp
    ${IMGUI_HOOK_DIR}/hooks.cpp
    ${IMGUI_HOOK_DIR}/inputhooks.cpp
    ${IMGUI_HOOK_DIR}/mousehooks.cpp
    ${IMGUI_HOOK_DIR}/d3d11hook.cpp
    ${IMGUI_HOOK_DIR}/d3d12hook.cpp
    ${IMGUI_HOOK_DIR}/imgui/imgui.cpp
    ${IMGUI_HOOK_DIR}/imgui/imgui_draw.cpp
    ${IMGUI_HOOK_DIR}/imgui/imgui_tables.cpp
    ${IMGUI_HOOK_DIR}/imgui/imgui_widgets.cpp
    ${IMGUI_HOOK_DIR}/imgui/imgui_impl_dx11.cpp
    ${IMGUI_HOOK_DIR}/imgui/imgui_impl_dx12.cpp
    ${IMGUI_HOOK_DIR}/imgui/imgui_impl_win32.cpp
    
    ${MINHOOK_DIR}/src/buffer.c
    ${MINHOOK_DIR}/src/hook.c
    ${MINHOOK_DIR}/src/trampoline.c
    ${MINHOOK_HDE_SOURCE}
)

set(HEADERS
    
    core/Types.h
    core/Logger.h
    core/PatternScanner.h
    core/ObjectArray.h
    core/NameResolution.h
    core/ObjectUtils.h
    core/WorldDiscovery.h
    core/EngineDetection.h
    core/EngineDiscovery.h
    core/GNativesResolver.h
    core/OffsetDiscovery.h
    core/FunctionFinder.h
    core/ObjectIndex.h
    core/BlueprintDiscovery.h
    core/ConsoleEnabler.h
    core/RuntimeConfig.h
    
    lua/LuaManager.h
    lua/LuaTypes.h
    lua/LuaUObject.h
    lua/LuaHookManager.h
    lua/LuaTypeGenerator.h
    lua/LuaCompatApi.h
    lua/BuiltinMods/BuiltinMods.h
    lua/BuiltinMods/UniversalHUD.h
    
    cppmods/CppModContext.h
    cppmods/CppModManager.h
    cppmods/CppModValue.h
    cppmods/CppModReflection.h
    cppmods/CppModHooks.h
    cppmods/CppModWidgets.h
    cppmods/CppModContainers.h
    cppmods/CppModNative.h
    cppmods/CppModImGui.h
    cppmods/sdk/Avalonium/ModApi.h
    cppmods/sdk/Avalonium/SdkApi.h
    cppmods/sdk/Avalonium/Api.hpp
    cppmods/sdk/Avalonium/Value.hpp
    cppmods/sdk/Avalonium/Log.hpp
    cppmods/sdk/Avalonium/Unreal.hpp
    cppmods/sdk/Avalonium/ModBase.hpp
    cppmods/sdk/Avalonium/Hooks.hpp
    cppmods/sdk/Avalonium/Keys.hpp
    cppmods/sdk/Avalonium/GameThread.hpp
    cppmods/sdk/Avalonium/Avalonium.hpp
    
    console/ConsoleInternal.h
    console/ConsoleVariables.h
    console/SeamlessTravelGuard.h
    
    api/UEInternalAPI.h
    api/IOCPServer.h
    api/IPCEvents.h
    api/IPCJobs.h
    api/IPCQuery.h
    
    overlay/OverlayDragMe.h
    overlay/SystemMetrics.h
    overlay/OverlayInternal.h
    
    usmap/USMapGenerator.h
    
    crypto/PakVerifier.h
    crypto/AESKeyFinder.h
    
    sdk/SDKGenerator.h
    sdk/SDKValidation.h
    
    utils/InlineHook.h
    utils/SafeMemory.h
    
    ${IMGUI_HOOK_DIR}/namespaces.h
    ${IMGUI_HOOK_DIR}/stdafx.h
    ${IMGUI_HOOK_DIR}/imgui/imconfig.h
    ${IMGUI_HOOK_DIR}/imgui/imgui.h
    ${IMGUI_HOOK_DIR}/imgui/imgui_internal.h
    ${IMGUI_HOOK_DIR}/imgui/imgui_impl_dx11.h
    ${IMGUI_HOOK_DIR}/imgui/imgui_impl_dx12.h
    ${IMGUI_HOOK_DIR}/imgui/imgui_impl_win32.h

    ${MINHOOK_DIR}/include/MinHook.h
)


add_library(Avalonium SHARED ${SOURCES} ${HEADERS})


target_include_directories(Avalonium SYSTEM PRIVATE
    ${IMGUI_HOOK_DIR}
    ${IMGUI_HOOK_DIR}/imgui
    ${MINHOOK_DIR}/include
    ${LUA_DIR}
)

target_include_directories(Avalonium PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MINHOOK_DIR}/src
    ${MINHOOK_DIR}/src/hde
)

target_compile_definitions(Avalonium PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    AVALONIUM_EXPORTS
    AVALONIUM_BUILD
    UNIVERSAL_MANAGER_BUILD
)

target_link_libraries(Avalonium PRIVATE
    kernel32 user32 gdi32 dwmapi psapi ws2_32 bcrypt shlwapi pdh
    dxgi d3d11 d3d12
)

if(MSVC AND WIN32)
    add_executable(AggressiveDog
        AggressiveDog/AggressiveDog.cpp
        AggressiveDog/version.rc
    )
    target_compile_definitions(AggressiveDog PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    target_compile_options(AggressiveDog PRIVATE /W4 /permissive-)
    target_link_libraries(AggressiveDog PRIVATE dbghelp psapi)
    set_target_properties(AggressiveDog PROPERTIES
        OUTPUT_NAME "AggressiveDog"
       
        RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_SOURCE_DIR}/DLL/CrashHandler>"
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )

    target_sources(Avalonium PRIVATE
        AggressiveDog/Launcher.cpp
        AggressiveDog/Launcher.h
    )
    add_dependencies(Avalonium AggressiveDog)
endif()


if(MSVC)
    target_compile_options(Avalonium PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/W4>
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
        /wd4505 /wd4244 /wd4100 /wd4189
    )

    target_link_options(Avalonium PRIVATE
        /DYNAMICBASE
        /NXCOMPAT
        /HIGHENTROPYVA
        /OPT:REF
        /OPT:ICF
    )

    set_source_files_properties(utils/SafeMemory.c PROPERTIES COMPILE_FLAGS "/EHa")
    
    set_source_files_properties(${IMGUI_HOOK_DIR}/d3d12hook.cpp PROPERTIES COMPILE_FLAGS "/wd4456")

    set_property(TARGET Avalonium PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
else()
    target_compile_options(Avalonium PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(Avalonium PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_command(TARGET Avalonium POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Avalonium>
    "${CMAKE_CURRENT_SOURCE_DIR}/DLL/Avalonium.dll"
    COMMENT "Deploying Avalonium.dll to Runtime/DLL/"
)

if(MSVC)
    add_custom_command(TARGET Avalonium POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/DLL/CrashHandler"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_PDB_FILE:Avalonium>"
        "${CMAKE_CURRENT_SOURCE_DIR}/DLL/CrashHandler/Avalonium.pdb"
        COMMENT "Deploying Avalonium.pdb beside AggressiveDog.exe"
    )
endif()

if(MSVC AND WIN32)
    enable_language(ASM_MASM)

    add_library(dwmapi_proxy SHARED
        proxy/dllmain_proxy.cpp
        proxy/dwmapi.asm
        proxy/dwmapi.def
        proxy/proxy_version.rc
    )

    set_target_properties(dwmapi_proxy PROPERTIES
        OUTPUT_NAME "dwmapi"
        PREFIX ""
        SUFFIX ".dll"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )

    target_include_directories(dwmapi_proxy PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/proxy"
        "${CMAKE_CURRENT_SOURCE_DIR}/utils"
    )

    target_compile_definitions(dwmapi_proxy PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )

    target_compile_options(dwmapi_proxy PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/W4>
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->
        $<$<COMPILE_LANGUAGE:CXX>:/wd4505>
        $<$<COMPILE_LANGUAGE:CXX>:/wd4100>
    )

    target_link_options(dwmapi_proxy PRIVATE 
        /IGNORE:4104 
        /DYNAMICBASE 
        /NXCOMPAT 
        /HIGHENTROPYVA
        /OPT:REF
        /OPT:ICF
    )
    
    target_link_libraries(dwmapi_proxy PRIVATE kernel32)
endif()
