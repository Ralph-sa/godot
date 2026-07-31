# CMakeLists.txt for Godot NAPI Bridge
# This builds libgodot_napi.so which bridges ArkTS → Godot Engine
# Architecture: libgodot_napi.so dlopen()s libgodot.so at runtime
# The dlopen runs on a worker thread to avoid main-thread ANR.

cmake_minimum_required(VERSION 3.5.0)
project(godot_napi)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

# NAPI shared library - thin wrapper only
add_library(godot_napi SHARED
    godot_napi_bridge.cpp
)

# OHOS SDK libraries needed by the bridge
find_library(HILOG_NDK_LIB hilog_ndk.z)
find_library(ACE_NDK_LIB ace_ndk.z)
find_library(ACE_NAPI_LIB ace_napi.z)

target_link_libraries(godot_napi PUBLIC
    ${HILOG_NDK_LIB}
    ${ACE_NDK_LIB}
    ${ACE_NAPI_LIB}
)

# pthread is built-in on OHOS (part of libc), no extra link needed.

# Linker options: ensure constructor functions and exported symbols survive LTO/GC
target_link_options(godot_napi PRIVATE
    -Wl,--export-dynamic
    -Wl,--no-gc-sections
)

# Compile options
target_compile_options(godot_napi PRIVATE
    -Wall
    -Wno-unused-parameter
    -fexceptions
    -frtti
    -pthread
)

# C++17 for smart pointers and std::atomic
set_target_properties(godot_napi PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
