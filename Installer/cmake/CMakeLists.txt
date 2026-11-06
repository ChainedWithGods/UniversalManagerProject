cmake_minimum_required(VERSION 3.20)

project(UniversalModManagerInstaller
    VERSION 1.0.0
    LANGUAGES C CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "This installer targets Windows only.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Please configure a 64-bit (x64) generator, e.g.:\n"
        "  cmake -G \"Visual Studio 17 2022\" -A x64 ..")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

if(MSVC)
    add_compile_options(/utf-8 /guard:cf /ZH:SHA_256)
    add_link_options(/GUARD:CF /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../build/cmake_out")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/../build/cmake_out")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/../build/cmake_out_dbg")

add_subdirectory(Installer)
add_subdirectory(Uninstaller)