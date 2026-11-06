# ── LZMA SDK (7-zip ANSI-C) static library ────────────────────────────────────
set(LZMA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lzmasdk/C")

add_library(lzma_sdk STATIC
    ${LZMA_DIR}/7zAlloc.c
    ${LZMA_DIR}/7zArcIn.c
    ${LZMA_DIR}/7zBuf.c
    ${LZMA_DIR}/7zBuf2.c
    ${LZMA_DIR}/7zCrc.c
    ${LZMA_DIR}/7zCrcOpt.c
    ${LZMA_DIR}/7zDec.c
    ${LZMA_DIR}/7zFile.c
    ${LZMA_DIR}/7zStream.c
    ${LZMA_DIR}/Bcj2.c
    ${LZMA_DIR}/Bra.c
    ${LZMA_DIR}/Bra86.c
    ${LZMA_DIR}/CpuArch.c
    ${LZMA_DIR}/Delta.c
    ${LZMA_DIR}/Lzma2Dec.c
    ${LZMA_DIR}/LzmaDec.c
)

target_include_directories(lzma_sdk PUBLIC ${LZMA_DIR})

# LZMA SDK is pure C — suppress MSVC warnings that don't apply to it
target_compile_options(lzma_sdk PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W3 /wd4146 /wd4244 /wd4267>
)

# Static runtime already forced at root level via CMAKE_MSVC_RUNTIME_LIBRARY

# ── Installer executable ───────────────────────────────────────────────────────
add_executable(UniversalModManager-Setup WIN32
    installer.cpp
    theme.cpp
    buttons.cpp
    WaveText.cpp
    installer.rc
)

set_target_properties(UniversalModManager-Setup PROPERTIES
    OUTPUT_NAME "UniversalModManager-Setup"
    PDB_NAME    "UniversalModManager-Setup"
)

target_link_libraries(UniversalModManager-Setup PRIVATE
    lzma_sdk
    comctl32
    shlwapi
    shell32
    dwmapi
    msimg32      # GradientFill (vertical gradients in theme.cpp)
    gdiplus      # anti-aliased badge drawing in theme.cpp (FillCircleAA / FillRoundRectAA)
    ole32
    advapi32
)

target_compile_definitions(UniversalModManager-Setup PRIVATE
    WIN32
    _WINDOWS
    UNICODE
    _UNICODE
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:Debug>:_DEBUG>
)

# ── Release-specific optimizations ────────────────────────────────────────────
target_compile_options(UniversalModManager-Setup PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /Oi /GL /Gy>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/Od /Zi>
)

target_link_options(UniversalModManager-Setup PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/LTCG /OPT:REF /OPT:ICF /DEBUG:NONE>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/DEBUG>
)

# ── Embed manifest ─────────────────────────────────────────────────────────────
# The manifest is embedded via the resource compiler (installer.rc includes it as RT_MANIFEST)
# Tell CMake not to generate its own manifest - we handle it in the .rc file
target_link_options(UniversalModManager-Setup PRIVATE
    /MANIFEST:NO
)
