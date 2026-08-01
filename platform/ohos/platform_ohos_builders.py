"""Functions used to generate source files during build time.

Godot HarmonyOS 平台构建辅助函数。
第 1 轮（骨架期）：仅提供 make_debug_ohos（分离调试符号），
后续轮次可追加 .hap 打包等构建辅助。
"""

import os
import shutil
import subprocess


def make_debug_ohos(target, source, env):
    """为 OHOS 产物分离调试符号（使用 llvm-strip / llvm-objcopy）。

    与 Android 的做法一致：将调试信息提取到 .debug 文件，再 strip 主产物，
    显著减小安装包体积（.hap 对体积敏感）。
    """
    dst = str(target[0])

    # 分离调试符号：llvm-objcopy --only-keep-debug
    objcopy = os.path.join(env["LLVM_BIN"] if "LLVM_BIN" in env else "", "llvm-objcopy")
    if not os.path.exists(objcopy):
        # 回退到 PATH 中的 llvm-objcopy
        objcopy = "llvm-objcopy"
    subprocess.run([objcopy, "--only-keep-debug", dst, f"{dst}.debug"], check=True)

    # strip 主产物
    strip = os.path.join(env["LLVM_BIN"] if "LLVM_BIN" in env else "", "llvm-strip")
    if not os.path.exists(strip):
        strip = "llvm-strip"
    subprocess.run([strip, "--strip-unneeded", dst], check=True)

    # 记录调试文件路径，供后续打包使用
    env.ohos_debug_file = f"{dst}.debug"
