import os
import platform
import sys
from typing import TYPE_CHECKING

from methods import print_error, print_warning
from platform_methods import validate_arch

if TYPE_CHECKING:
    from SCons.Script.SConscript import SConsEnvironment


def get_name():
    return "HarmonyOS"


def can_build():
    return os.path.exists(get_env_ohos_sdk_root())


def get_tools(env: "SConsEnvironment"):
    return ["clang", "clang++", "as", "ar", "link"]


def get_opts():
    from SCons.Variables import BoolVariable

    return [
        ("OHOS_SDK_HOME", "Path to the HarmonyOS SDK", get_env_ohos_sdk_root()),
        BoolVariable("ohos_editor_build", "Build the Godot editor for HarmonyOS", True),
    ]


def get_doc_classes():
    return [
        "EditorExportPlatformHarmonyOS",
    ]


def get_doc_path():
    return "doc_classes"


def get_env_ohos_sdk_root():
    return os.environ.get("OHOS_SDK_HOME", "")


def get_flags():
    return {
        "arch": "arm64",
        "target": "template_debug",
        "supported": [],
    }


def configure(env: "SConsEnvironment"):
    # Validate arch
    supported_arches = ["arm64"]
    validate_arch(env["arch"], get_name(), supported_arches)

    ohos_sdk = env["OHOS_SDK_HOME"]

    if not ohos_sdk:
        print_error("OHOS_SDK_HOME environment variable is not set. "
                     "Please set it to your HarmonyOS SDK path.")
        sys.exit(255)

    # Toolchain path (normalize to forward slashes for SCons compatibility)
    toolchain_path = os.path.join(ohos_sdk, "native", "llvm", "bin").replace("\\", "/")

    if not os.path.exists(toolchain_path):
        print_error(f"Cannot find OHOS toolchain at '{toolchain_path}'. "
                     "Please ensure OHOS_SDK_HOME is correct and the native toolchain is installed.")
        sys.exit(255)

    # Architecture: only arm64 (Kirin X90)
    # Note: wrapper scripts use aarch64-unknown-linux-ohos but sysroot uses aarch64-linux-ohos.
    # Using aarch64-linux-ohos for sysroot compatibility.
    target_triple = "aarch64-linux-ohos"
    target_option = ["--target=" + target_triple]
    env.Append(ASFLAGS=target_option)
    env.Append(CCFLAGS=target_option)
    env.Append(LINKFLAGS=target_option)

    # LTO
    if env["lto"] == "auto":
        env["lto"] = "none"

    if env["lto"] != "none":
        if env["lto"] == "thin":
            env.Append(CCFLAGS=["-flto=thin"])
            env.Append(LINKFLAGS=["-flto=thin"])
        else:
            env.Append(CCFLAGS=["-flto"])
            env.Append(LINKFLAGS=["-flto"])

    # Compiler configuration
    env["SHLIBSUFFIX"] = ".so"

    if env["PLATFORM"] == "win32":
        env.use_windows_spawn_fix()

    # Determine toolchain host platform
    if sys.platform.startswith("linux"):
        host_subpath = "linux-x86_64"
    elif sys.platform.startswith("darwin"):
        host_subpath = "darwin-x86_64"
    elif sys.platform.startswith("win"):
        host_subpath = "windows-x86_64"

    compiler_path = toolchain_path

    # Use clang.exe directly with --target flag, since the wrapper scripts
    # (aarch64-unknown-linux-ohos-clang) are shell scripts that Windows cannot execute.
    clang_exe = os.path.join(compiler_path, "clang.exe").replace("\\", "/")
    env["CC"] = clang_exe
    env["CXX"] = (os.path.join(compiler_path, "clang++.exe")).replace("\\", "/")
    env["AR"] = (os.path.join(compiler_path, "llvm-ar.exe")).replace("\\", "/")
    env["RANLIB"] = (os.path.join(compiler_path, "llvm-ranlib.exe")).replace("\\", "/")
    env["AS"] = clang_exe

    # Sysroot (normalize to forward slashes)
    sysroot = os.path.join(ohos_sdk, "native", "sysroot").replace("\\", "/")
    env.Append(CCFLAGS=[f"--sysroot={sysroot}"])
    env.Append(LINKFLAGS=[f"--sysroot={sysroot}"])

    # Compile flags
    env.Append(
        CCFLAGS=(["-fpic", "-ffunction-sections", "-funwind-tables",
                  "-fstack-protector-strong", "-fvisibility=hidden"])
    )

    # ARM64 specific
    env.Append(CCFLAGS=["-march=armv8-a"])
    env.Append(CPPDEFINES=["__ARM_ARCH_8A__"])
    env.Append(CPPDEFINES=[("_FILE_OFFSET_BITS", 64)])

    env.Append(CCFLAGS=["-ffp-contract=off"])

    # Link flags
    env.Append(LINKFLAGS=["-Wl,--gc-sections", "-Wl,--no-undefined", "-Wl,-z,now"])
    env.Append(LINKFLAGS=["-Wl,--build-id"])
    env.Append(LINKFLAGS=["-Wl,-soname,libgodot_harmonyos.so"])

    # Platform include paths
    env.Prepend(CPPPATH=["#platform/harmonyos"])
    # Add OHOS SDK include paths for native headers
    env.Append(CPPPATH=[
        os.path.join(sysroot, "usr", "include"),
    ])

    # Platform defines
    env.Append(CPPDEFINES=[
        "HARMONYOS_ENABLED",
        "UNIX_ENABLED",
        "__OHOS__",
        "VULKAN_ENABLED",
        "RD_ENABLED",
    ])

    # OHOS system libraries
    env.Append(LIBS=[
        "native_window",     # XComponent NativeWindow
        "hilog_ndk.z",       # HiLog
        "ace_ndk.z",         # ArkUI Native
        "ohaudio",           # OHAudio
        "z",                 # zlib
        "m",                 # math
    ])

    # Vulkan
    if env["vulkan"]:
        if not env["use_volk"]:
            env.Append(LIBS=["vulkan"])

    # OpenGL ES3 (fallback)
    if env["opengl3"]:
        env.Append(CPPDEFINES=["GLES3_ENABLED"])
        env.Append(LIBS=["GLESv3"])
