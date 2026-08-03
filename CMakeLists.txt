cmake_minimum_required(VERSION 3.10)
project(OracleHelper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W3 /utf-8)
    # Use Windows subsystem (no console window)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
endif()

# kissfft (header-only)
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/kissfft)

add_executable(OracleHelper
    src/main.cpp
    src/AudioAnalyzer.cpp
    src/OverlayWindow.cpp
    src/WavLoader.cpp
    src/kissfft.c
)

target_link_libraries(OracleHelper PRIVATE winmm)

target_include_directories(OracleHelper PRIVATE ${CMAKE_SOURCE_DIR}/src)
