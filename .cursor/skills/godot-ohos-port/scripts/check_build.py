#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_build.py —— Godot 鸿蒙平台移植：编译校验脚本

功能：
1. 优先调用 SCons 做真实交叉编译校验：scons platform=ohos target=editor arch=arm64
2. 若环境缺少 DEVECO_SDK_HOME / SCons，则降级为语法检查（g++ -fsyntax-only），
   并在报告中明确标注校验级别（real / syntax_only）。
3. 报告结果写入 stdout，供管理者/审查者作为轮次门槛依据。

用法：
    python3 check_build.py            # 完整交叉编译（需 SDK）
    python3 check_build.py --syntax   # 仅语法检查（无 SDK 时自动降级）
"""
import argparse
import os
import shutil
import subprocess
import sys

# 项目根目录（脚本位于 godot/.cursor/skills/godot-ohos-port/scripts/，上溯 4 级即 godot 根）
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SKILL_DIR = os.path.dirname(SCRIPT_DIR)            # godot-ohos-port/
CURSOR_DIR = os.path.dirname(SKILL_DIR)            # .cursor/
GODOT_DIR = os.path.dirname(os.path.dirname(CURSOR_DIR))  # godot/

# 默认 SDK 探测路径（DevEco Studio 或 Command Line Tools）
DEVECO_SDK_CANDIDATES = [
    "/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony",
    os.path.expanduser("~/command-line-tools/sdk/default/openharmony"),
]


def find_sdk_home() -> str:
    """探测 DEVECO_SDK_HOME，返回 SDK native 目录；未找到返回空串。"""
    def has_clang(sdk: str) -> bool:
        return os.path.isfile(os.path.join(sdk, "native", "llvm", "bin", "clang"))

    env = os.environ.get("DEVECO_SDK_HOME", "")
    if env and has_clang(env):
        return env
    for cand in DEVECO_SDK_CANDIDATES:
        if has_clang(cand):
            return cand
    return ""


def run_scons_build(sdk_home: str, verbose: bool = False) -> tuple[bool, str]:
    """执行 SCons 交叉编译，返回 (是否通过, 输出摘要)。"""
    env = os.environ.copy()
    clang = os.path.join(sdk_home, "native", "llvm", "bin", "clang")
    sysroot = os.path.join(sdk_home, "native", "sysroot")
    env["DEVECO_SDK_HOME"] = sdk_home
    env["CC"] = f'"{clang}" --target=aarch64-linux-ohos'
    env["CXX"] = f'"{clang}++" --target=aarch64-linux-ohos'
    env["CFLAGS"] = f"--sysroot={sysroot} -fPIC -D__MUSL__=1"
    env["CXXFLAGS"] = env["CFLAGS"]

    cmd = ["scons", "platform=ohos", "target=editor", "arch=arm64"]
    # SCons 可能安装在用户 Python 目录（如 ~/Library/Python/3.9/bin/scons）
    scons_exe = shutil.which("scons")
    if not scons_exe:
        for cand in [
            os.path.expanduser("~/Library/Python/3.9/bin/scons"),
            os.path.expanduser("~/Library/Python/3.11/bin/scons"),
            "/opt/homebrew/bin/scons",
        ]:
            if os.path.isfile(cand):
                scons_exe = cand
                break
    if not scons_exe:
        return False, "未找到 scons 命令，请先安装 scons"
    cmd[0] = scons_exe
    print(f"[check_build] 执行：{' '.join(cmd)}")
    try:
        proc = subprocess.run(
            cmd, cwd=GODOT_DIR, env=env, capture_output=True, text=True, timeout=1800
        )
    except subprocess.TimeoutExpired:
        return False, "编译超时（>30 分钟）"
    except FileNotFoundError:
        return False, "未找到 scons 命令，请先安装 scons"

    summary = proc.stdout[-4000:] if proc.stdout else ""
    ok = proc.returncode == 0
    return ok, summary


def run_syntax_check() -> tuple[bool, str]:
    """无 SDK 时降级：对 platform/ohos 下 .cpp 文件做 g++ -fsyntax-only 检查。"""
    ohos_dir = os.path.join(GODOT_DIR, "platform", "ohos")
    if not os.path.isdir(ohos_dir):
        return False, "platform/ohos 目录不存在，尚无代码可检查"

    files = []
    for root, _, names in os.walk(ohos_dir):
        for name in names:
            if name.endswith((".cpp", ".cc")):
                files.append(os.path.join(root, name))
    if not files:
        return False, "platform/ohos 下无 C++ 源文件"

    # 用 g++/clang++ 做纯语法检查（-fsyntax-only），不依赖 SDK 头文件
    compiler = shutil.which("clang++") or shutil.which("g++")
    failed = []
    for f in sorted(files):
        proc = subprocess.run(
            [compiler, "-fsyntax-only", "-std=c++17", f],
            capture_output=True, text=True,
        )
        if proc.returncode != 0:
            failed.append(f)

    if failed:
        detail = "\n".join(failed[:10])
        return False, f"语法检查失败（{len(failed)} 个文件）：\n{detail}"
    return True, f"语法检查通过（{len(files)} 个文件）"


def main() -> int:
    """主入口：选择真实编译或语法检查，输出报告。"""
    parser = argparse.ArgumentParser(description="Godot OHOS 编译校验")
    parser.add_argument("--syntax", action="store_true", help="强制仅做语法检查")
    args = parser.parse_args()

    sdk_home = find_sdk_home()
    if args.syntax or not sdk_home:
        # 环境无 SDK：降级语法检查，并在报告中明确标注
        level = "syntax_only"
        ok, detail = run_syntax_check()
    else:
        level = "real"
        ok, detail = run_scons_build(sdk_home)

    print("=" * 60)
    print("check_build 报告")
    print("=" * 60)
    print(f"校验级别：{level}（real=完整交叉编译 / syntax_only=语法降级）")
    print(f"结果：{'通过' if ok else '失败'}")
    if detail:
        print(f"详情：\n{detail}")
    print("=" * 60)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
