#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
diff_summary.py —— Godot 鸿蒙平台移植：macOS ↔ OHOS 对比总结脚本

功能（双维度对比，每轮对比总结必用）：
1. 行数统计：逐文件对比 platform/macos/* 与 platform/ohos/*，输出行数完成度百分比；
2. 接口覆盖率：解析 macOS 头文件的纯虚函数/公开方法声明，检查 OHOS 头文件是否逐一
   override（对 DisplayServer / OS / RenderingContextDriver 等基类关键）；
3. 输出 markdown 报告，供管理者写入 round-progress.md 与 porting-matrix.md。

用法：
    python3 diff_summary.py            # 完整对比，输出 markdown 报告
    python3 diff_summary.py --brief    # 仅输出统计摘要
"""
import argparse
import os
import re
import sys

# 脚本位于 godot/.cursor/skills/godot-ohos-port/scripts/，上溯 4 级即 godot 根
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SKILL_DIR = os.path.dirname(SCRIPT_DIR)
CURSOR_DIR = os.path.dirname(SKILL_DIR)
GODOT_DIR = os.path.dirname(os.path.dirname(CURSOR_DIR))

MACOS_DIR = os.path.join(GODOT_DIR, "platform", "macos")
OHOS_DIR = os.path.join(GODOT_DIR, "platform", "ohos")

# 需要接口覆盖率核查的关键基类头文件
KEY_BASE_CLASSES = [
    "os_macos.h",                 # OS_OHOS : OS_Unix
    "display_server_macos.h",     # DisplayServerOHOS : DisplayServer
    "rendering_context_driver_vulkan_macos.h",  # RenderingContextDriverVulkanOHOS
]


def count_lines(path: str) -> int:
    """统计文件行数；文件不存在返回 0。"""
    if not os.path.isfile(path):
        return 0
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        return sum(1 for _ in f)


def list_files(directory: str) -> list[str]:
    """递归列出目录下所有文件（相对路径，排序）。"""
    if not os.path.isdir(directory):
        return []
    result = []
    for root, _, names in os.walk(directory):
        for name in names:
            full = os.path.join(root, name)
            result.append(os.path.relpath(full, directory))
    return sorted(result)


def extract_declared_methods(header_path: str) -> list[str]:
    """从 C++ 头文件提取公开方法名清单（近似，用于接口覆盖率统计）。"""
    if not os.path.isfile(header_path):
        return []
    methods = set()
    with open(header_path, "r", encoding="utf-8", errors="ignore") as f:
        content = f.read()
    # 匹配形如 返回类型 方法名( 或 ~析构 的函数声明（忽略 const 后等修饰）
    pattern = re.compile(
        r"(?:virtual\s+)?[~\w:]+\s+(\w+)\s*\(", re.MULTILINE
    )
    for m in pattern.finditer(content):
        name = m.group(1)
        # 过滤明显不是成员函数的关键字
        if name in ("if", "for", "while", "return", "sizeof", "switch", "static_cast",
                    "reinterpret_cast", "const_cast", "dynamic_cast", "alignof", "decltype"):
            continue
        if len(name) > 1:  # 过滤单字符变量
            methods.add(name)
    return sorted(methods)


def build_report(brief: bool) -> str:
    """构建对比报告（markdown）。"""
    macos_files = list_files(MACOS_DIR)
    ohos_files = list_files(OHOS_DIR)

    macos_total = sum(count_lines(os.path.join(MACOS_DIR, f)) for f in macos_files)
    ohos_total = sum(count_lines(os.path.join(OHOS_DIR, f)) for f in ohos_files)

    # 同名文件映射（简单按文件名匹配；macOS .mm → ohos .cpp 视为同名）
    def base_name(rel: str) -> str:
        base = os.path.basename(rel)
        return base.replace(".mm", "").replace(".cpp", "")

    ohos_bases = {base_name(f): f for f in ohos_files}
    ported_files = []
    not_ported = []
    for f in macos_files:
        if base_name(f) in ohos_bases:
            ported_files.append(f)
        else:
            not_ported.append(f)

    # 行数完成度：按已映射文件的行数占比估算
    ported_lines = sum(count_lines(os.path.join(MACOS_DIR, f)) for f in ported_files)
    line_completion = (ported_lines / macos_total * 100) if macos_total else 0

    # 接口覆盖率：关键基类头文件的方法名在 OHOS 侧是否出现
    iface_covered = 0
    iface_total = 0
    for base in KEY_BASE_CLASSES:
        macos_h = os.path.join(MACOS_DIR, base)
        methods = extract_declared_methods(macos_h)
        if not methods:
            continue
        iface_total += len(methods)
        ohos_target = os.path.join(OHOS_DIR, base.replace("macos", "ohos"))
        ohos_content = ""
        if os.path.isfile(ohos_target):
            with open(ohos_target, "r", encoding="utf-8", errors="ignore") as f:
                ohos_content = f.read()
        for m in methods:
            if m in ohos_content:
                iface_covered += 1
    iface_coverage = (iface_covered / iface_total * 100) if iface_total else 0

    if brief:
        return (
            f"macOS={len(macos_files)}文件/{macos_total}行 | "
            f"OHOS={len(ohos_files)}文件/{ohos_total}行 | "
            f"已映射={len(ported_files)} | 行数完成度={line_completion:.1f}% | "
            f"接口覆盖率={iface_coverage:.1f}% ({iface_covered}/{iface_total})"
        )

    lines = []
    lines.append("## diff_summary：macOS ↔ OHOS 对比报告\n")
    lines.append("| 指标 | 数值 |")
    lines.append("|---|---|")
    lines.append(f"| macOS 文件数 | {len(macos_files)} |")
    lines.append(f"| macOS 总行数 | {macos_total} |")
    lines.append(f"| OHOS 文件数 | {len(ohos_files)} |")
    lines.append(f"| OHOS 总行数 | {ohos_total} |")
    lines.append(f"| 已映射文件数 | {len(ported_files)} |")
    lines.append(f"| 未映射文件数 | {len(not_ported)} |")
    lines.append(f"| 行数完成度 | {line_completion:.1f}% |")
    lines.append(f"| 接口覆盖率 | {iface_coverage:.1f}% ({iface_covered}/{iface_total}) |")
    lines.append("")

    if ported_files:
        lines.append("### 已映射文件")
        lines.append("")
        for f in ported_files:
            lines.append(f"- {f}")
        lines.append("")

    if not_ported:
        lines.append("### 未映射文件（需判定：移植/不需要/可选）")
        lines.append("")
        for f in not_ported:
            lines.append(f"- {f}")
        lines.append("")

    return "\n".join(lines)


def main() -> int:
    """主入口：输出对比报告。"""
    parser = argparse.ArgumentParser(description="macOS ↔ OHOS 对比总结")
    parser.add_argument("--brief", action="store_true", help="仅输出统计摘要")
    args = parser.parse_args()

    report = build_report(brief=args.brief)
    print(report)
    return 0


if __name__ == "__main__":
    sys.exit(main())
