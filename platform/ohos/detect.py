#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
detect.py —— Godot HarmonyOS 平台构建检测与配置

功能：
1. can_build()：检测 HarmonyOS SDK（DEVECO_SDK_HOME）是否存在，决定该平台可否构建；
2. get_opts()：暴露 SDK 路径、目标 API 版本等 SCons 选项；
3. get_flags()：声明默认架构 arm64、默认目标 editor、启用 Vulkan；
4. configure()：配置 OHOS 的 clang 交叉编译工具链（aarch64-linux-ohos + musl sysroot），
   设置编译宏与链接库。

本文件参照 platform/android/detect.py 与 platform/macos/detect.py 的写法，
针对 HarmonyOS 6.1.1（SDK 24 / API 24 Release）做交叉编译适配。

用法：
    scons platform=ohos target=editor arch=arm64
"""
import os
import sys
from typing import TYPE_CHECKING

from methods import print_error, print_warning
from platform_methods import validate_arch

if TYPE_CHECKING:
    from SCons.Script.SConscript import SConsEnvironment

# 目标三元组（OHOS 使用 musl libc，动态链接器 /lib/ld-musl-aarch64.so.1）
OHOS_TARGET_TRIPLE = "aarch64-linux-ohos"


def get_name():
    """平台名，用于 SCons 显示与产物命名。"""
    return "HarmonyOS"


def _detect_sdk_home():
    """探测 HarmonyOS SDK native 目录。

    优先读取环境变量 DEVECO_SDK_HOME；否则探测 DevEco Studio 或 Command Line Tools 的默认路径。
    返回 SDK native 目录的绝对路径；未找到返回空串。
    """
    candidates = []
    env = os.environ.get("DEVECO_SDK_HOME", "")
    if env:
        candidates.append(env)
    # DevEco Studio 6.1.1 内置 SDK 路径
    candidates.append("/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony")
    # Command Line Tools 6.1.1 解压路径（~/.zshrc 的 HMOS_HOME 指向 ~/HarmonyOS/command-line-tools）
    candidates.append(os.path.expanduser("~/HarmonyOS/command-line-tools/sdk/default/openharmony"))
    # 旧版路径（已迁移，保留兼容）
    candidates.append(os.path.expanduser("~/command-line-tools/sdk/default/openharmony"))
    for cand in candidates:
        if os.path.isfile(os.path.join(cand, "native", "llvm", "bin", "clang")):
            return cand
    return ""


def can_build():
    """能否构建：要求存在 HarmonyOS SDK 的 clang 交叉编译器。"""
    return os.path.exists(_detect_sdk_home())


def get_tools(env: "SConsEnvironment"):
    """声明构建工具：与 Android 一样使用 clang 系列工具。"""
    return ["clang", "clang++", "as", "ar", "link"]


def get_opts():
    """暴露 SCons 命令行选项（--sdk 等）。"""
    from SCons.Variables import EnumVariable

    return [
        ("DEVECO_SDK_HOME", "Path to the HarmonyOS SDK (native folder parent)", _detect_sdk_home()),
        ("target_api", "Target HarmonyOS API level (24 = HarmonyOS 6.1.1)", "24"),
        EnumVariable("platform", "Target platform (ohos)", "ohos", ["ohos"]),
    ]


def get_doc_classes():
    """文档类列表，用于导出器文档生成。"""
    return [
        "EditorExportPlatformOHOS",
    ]


def get_doc_path():
    """文档目录。"""
    return "doc_classes"


def get_flags():
    """默认构建参数：arm64、编辑器、Vulkan 渲染后端、共享库产物。

    - OHOS 无 main 入口：应用由 ArkTS UIAbility 驱动，C++ 侧是 NAPI 模块，
      因此默认构建为共享库 libgodot.so（与 Android 的 libgodot 一致）；
    - OHOS 仅支持 Vulkan（OpenGL/EGL 无官方支持），因此显式禁用 opengl3，
      避免编译 GLES3 驱动时缺失 platform_gl.h。
    """
    return {
        "arch": "arm64",
        "target": "editor",
        "vulkan": True,
        "opengl3": False,
        "library_type": "shared_library",
        "supported": ["mono", "library"],
    }


def configure(env: "SConsEnvironment"):
    """配置交叉编译环境：OHOS clang + musl sysroot + 宏 + 链接库。"""
    # 校验架构：HarmonyOS PC/2in1 目标为 arm64
    supported_arches = ["arm64"]
    validate_arch(env["arch"], get_name(), supported_arches)

    sdk_home = env["DEVECO_SDK_HOME"]
    if not sdk_home or not os.path.isdir(sdk_home):
        print_error(
            "HarmonyOS SDK not found. Please set DEVECO_SDK_HOME to the OHOS SDK path, e.g.\n"
            '  export DEVECO_SDK_HOME="/Users/toro/HarmonyOS/command-line-tools/sdk/default/openharmony"'
        )
        sys.exit(255)

    native_dir = os.path.join(sdk_home, "native")
    llvm_bin = os.path.join(native_dir, "llvm", "bin")
    sysroot = os.path.join(native_dir, "sysroot")

    # 工具链：OHOS 自带的 clang，目标三元组 aarch64-linux-ohos
    env["CC"] = os.path.join(llvm_bin, "clang")
    env["CXX"] = os.path.join(llvm_bin, "clang++")
    env["AR"] = os.path.join(llvm_bin, "llvm-ar")
    env["RANLIB"] = os.path.join(llvm_bin, "llvm-ranlib")
    env["AS"] = os.path.join(llvm_bin, "clang")
    env["LD"] = os.path.join(llvm_bin, "ld.lld")

    target_option = ["-target", OHOS_TARGET_TRIPLE]
    sysroot_option = ["--sysroot=" + sysroot]

    env.Append(ASFLAGS=[target_option, "-c"])
    env.Append(CCFLAGS=target_option + sysroot_option)
    env.Append(LINKFLAGS=target_option + sysroot_option)

    # musl 环境需要 -fPIC 与 __MUSL__ 宏（部分第三方库据此分支）
    env.Append(CCFLAGS=["-fPIC", "-ffunction-sections", "-funwind-tables", "-fvisibility=hidden"])
    env.Append(CPPDEFINES=["__MUSL__=1", "_GNU_SOURCE"])

    # 共享库后缀 .so
    env["SHLIBSUFFIX"] = ".so"

    # LTO：与 Android 一致，默认关闭
    if env["lto"] == "auto":
        env["lto"] = "none"

    # 平台宏：OHOS_ENABLED / UNIX_ENABLED / VULKAN_ENABLED / RD_ENABLED
    env.Prepend(CPPPATH=["#platform/ohos"])
    env.Append(CPPDEFINES=["OHOS_ENABLED", "UNIX_ENABLED"])

    # 链接库：OHOS 系统库（native_window / vulkan / ohaudio / inputmethod / hilog_ndk / ace_ndk / z / dl / pthread）
    # 注意：hilog_ndk、ace_ndk 与 ace_napi 在 OHOS SDK 中命名为 *.z.so（ELF 格式），
    # 需用 -l:精确文件名 链接；其余为标准 .so/.a。
    env.Append(LIBS=["native_window", "vulkan", "ohaudio", "ohinputmethod", "z", "dl", "pthread"])
    env.Append(LINKFLAGS=["-l:libhilog_ndk.z.so", "-l:libace_ndk.z.so", "-l:libace_napi.z.so", "-l:librawfile.z.so"])

    # GLES3：OHOS 无 EGL 官方支持，强制禁用（即使命令行传入 opengl3=yes）
    if env["opengl3"]:
        print("Note: OHOS has no OpenGL/EGL support; ignoring opengl3 option.")
    env["opengl3"] = False

    if env["vulkan"]:
        env.Append(CPPDEFINES=["VULKAN_ENABLED", "RD_ENABLED"])
        # OHOS 场景禁用 volk：volk.h 未适配 VK_OHOS_surface 的类型声明，
        # 直接使用 SDK 的 vulkan.h（其已无条件包含 vulkan_ohos.h）。
        env["use_volk"] = False
        env.Append(LIBS=["vulkan"])

    if env["opengl3"]:
        env.Append(CPPDEFINES=["GLES3_ENABLED"])
        env.Append(LIBS=["GLESv3"])

    # 工具链版本打印（便于构建日志排查）
    print("Building for HarmonyOS (aarch64-linux-ohos), sysroot=%s" % sysroot)
