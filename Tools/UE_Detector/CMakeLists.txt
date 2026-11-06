cmake_minimum_required(VERSION 3.20)
project(UEVersionDetector VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(UE_Detector src/main.cpp src/pe_parser.cpp src/version_detector.cpp version.rc)

target_include_directories(UE_Detector PRIVATE src ${CMAKE_SOURCE_DIR})


target_link_libraries(UE_Detector version)

if(MSVC)
    target_compile_options(UE_Detector PRIVATE /W4 /permissive-)
else()
    target_compile_options(UE_Detector PRIVATE -Wall -Wextra -Wpedantic)
endif()