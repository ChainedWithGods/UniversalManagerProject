# ── Uninstaller executable ────────────────────────────────────────────────────
add_executable(uninstall WIN32
    uninstaller.cpp
    uninstaller.rc
)

set_target_properties(uninstall PROPERTIES
    OUTPUT_NAME "uninstall"
    PDB_NAME    "uninstall"
)

target_compile_definitions(uninstall PRIVATE
    WIN32
    _WINDOWS
    UNICODE
    _UNICODE
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:Debug>:_DEBUG>
)

target_link_libraries(uninstall PRIVATE
    user32
    comctl32
    dwmapi
    shell32
    ole32
    advapi32
    shlwapi
)

# ── Release-specific optimizations & Security Hardening ───────────────────────
target_compile_options(uninstall PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
    "$<$<CXX_COMPILER_ID:MSVC>:/guard:cf>"
    "$<$<CXX_COMPILER_ID:MSVC>:/ZH:SHA_256>"
    "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2;/Oi;/GL;/Gy>"
    "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Od;/Zi>"
)

target_link_options(uninstall PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/GUARD:CF;/DYNAMICBASE;/NXCOMPAT;/HIGHENTROPYVA;/MANIFEST:NO>"
    "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/LTCG;/OPT:REF;/OPT:ICF>"
    "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/DEBUG>"
)