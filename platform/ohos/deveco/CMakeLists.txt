# CMakeLists.txt —— Godot HarmonyOS 引擎原生库编译配置
#
# 该文件由 DevEco Studio 工程中的 native 模块（ohos-ndk CMake）引用，
# 负责将 Godot 引擎源码（platform/ohos 平台 + 引擎核心）编译为 libgodot.so，
# 供 ArkTS 通过 NAPI 调用（moduleName = "godot"）。
#
# 用法：在 DevEco 工程的 entry/src/main/cpp/CMakeLists.txt 中
#   include(/path/to/godot/platform/ohos/deveco/CMakeLists.txt)
#
# 第 1 轮（骨架期）：声明编译配置与源文件收集规则；
# 实际编译流程（源码树集成 + 全量源文件列表）在完善期接入 SCons 产物。

cmake_minimum_required(VERSION 3.16)

# 引擎根目录（由调用方传入或环境变量提供）
if(NOT DEFINED GODOT_SOURCE_DIR)
    set(GODOT_SOURCE_DIR "$ENV{GODOT_SOURCE_DIR}" CACHE PATH "Godot engine source root")
endif()

# 原生库目标：libgodot.so（供 NAPI 模块 "godot" 使用）
add_library(godot SHARED
    # 平台层源码（骨架期：仅基础文件，后续按 SCons 清单同步）
    ${GODOT_SOURCE_DIR}/platform/ohos/main_ohos.cpp
    ${GODOT_SOURCE_DIR}/platform/ohos/os_ohos.cpp
    ${GODOT_SOURCE_DIR}/platform/ohos/display_server_ohos.cpp
    ${GODOT_SOURCE_DIR}/platform/ohos/ohos_xcomponent.cpp
    ${GODOT_SOURCE_DIR}/platform/ohos/ohos_window.cpp
    ${GODOT_SOURCE_DIR}/platform/ohos/crash_handler_ohos.cpp
)

# 引擎核心：完整 Godot 源码需通过 SCons 交叉编译为静态库后链接。
# 骨架期先声明依赖关系，完整构建脚本在完善期提供。
target_include_directories(godot PRIVATE
    ${GODOT_SOURCE_DIR}
    ${GODOT_SOURCE_DIR}/platform/ohos
    ${GODOT_SOURCE_DIR}/thirdparty/vulkan/include
    ${OHOS_SDK_NATIVE}/sysroot/usr/include
)

# 链接系统库（与 detect.py 保持一致）
target_link_libraries(godot PRIVATE
    native_window
    hilog
    ace_ndk
    z
    dl
    pthread
    vulkan
)

# NAPI 模块注册标志
target_compile_definitions(godot PRIVATE
    OHOS_ENABLED
    UNIX_ENABLED
    VULKAN_ENABLED
    RD_ENABLED
    __MUSL__=1
    _GNU_SOURCE
)
