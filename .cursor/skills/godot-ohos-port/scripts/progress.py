#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
progress.py —— Godot 鸿蒙平台移植：轮次进度推进脚本

功能：
1. 读取 round-progress.md 的 frontmatter（current_round / completed_rounds /
   total_completion / interface_coverage）；
2. advance：将 current_round +1，把当前轮次计入 completed_rounds；
3. set：直接设置各字段（每轮对比总结后用 diff_summary.py 结果更新完成度）。

用法：
    python3 progress.py status                  # 查看当前进度
    python3 progress.py advance                 # 推进到下一轮
    python3 progress.py set --round 3 --completion 45 --iface 30
"""
import argparse
import re
import sys
from pathlib import Path

# round-progress.md 位于技能根目录
SCRIPT_DIR = Path(__file__).resolve().parent
SKILL_DIR = SCRIPT_DIR.parent
PROGRESS_FILE = SKILL_DIR / "round-progress.md"

# 支持的中文字段映射：yaml 键 → frontmatter 行内 key
FIELD_KEYS = {
    "round": "current_round",
    "completion": "total_completion",
    "iface": "interface_coverage",
}


def read_frontmatter() -> dict:
    """解析 round-progress.md 的 frontmatter（--- 包裹的 yaml 块）。"""
    text = PROGRESS_FILE.read_text(encoding="utf-8")
    m = re.match(r"^---\n(.*?)\n---", text, re.DOTALL)
    if not m:
        raise RuntimeError(f"未找到 frontmatter：{PROGRESS_FILE}")
    fields = {}
    for line in m.group(1).splitlines():
        if ":" in line:
            k, v = line.split(":", 1)
            fields[k.strip()] = v.strip()
    return fields


def write_frontmatter(fields: dict) -> None:
    """写回 frontmatter，保持文件其余部分不变。"""
    text = PROGRESS_FILE.read_text(encoding="utf-8")
    m = re.match(r"^(---\n)(.*?)(\n---)", text, re.DOTALL)
    if not m:
        raise RuntimeError("frontmatter 解析失败")
    body = "\n".join(f"{k}: {v}" for k, v in fields.items())
    new_text = text[: m.start(2)] + body + text[m.start(3):]
    PROGRESS_FILE.write_text(new_text, encoding="utf-8")


def cmd_status() -> int:
    """打印当前进度状态。"""
    fields = read_frontmatter()
    print("round-progress 当前状态：")
    for k, v in fields.items():
        print(f"  {k}: {v}")
    return 0


def cmd_advance() -> int:
    """推进轮次：current_round +1，记录到 completed_rounds。"""
    fields = read_frontmatter()
    cur = int(fields.get("current_round", "0"))
    completed = fields.get("completed_rounds", "[]").strip("[]").replace(" ", "")
    rounds = [r for r in completed.split(",") if r]

    if cur >= 10:
        print("已达第 10 轮（终轮），无需推进；请产出 final-summary.md。")
        return 0

    new_round = cur + 1
    fields["current_round"] = str(new_round)
    if str(new_round) not in rounds:
        rounds.append(str(new_round))
    fields["completed_rounds"] = "[" + ", ".join(rounds) + "]"
    write_frontmatter(fields)
    print(f"已推进到第 {new_round} 轮。completed_rounds={rounds}")
    return 0


def cmd_set(args) -> int:
    """设置轮次/完成度/接口覆盖率字段。"""
    fields = read_frontmatter()
    if args.round is not None:
        fields[FIELD_KEYS["round"]] = str(args.round)
    if args.completion is not None:
        fields[FIELD_KEYS["completion"]] = f"{args.completion}%"
    if args.iface is not None:
        fields[FIELD_KEYS["iface"]] = f"{args.iface}%"
    write_frontmatter(fields)
    cmd_status()
    return 0


def main() -> int:
    """子命令分发。"""
    parser = argparse.ArgumentParser(description="round-progress.md 轮次推进")
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("status", help="查看当前进度")

    sub.add_parser("advance", help="推进到下一轮")

    p_set = sub.add_parser("set", help="设置字段值")
    p_set.add_argument("--round", type=int, help="当前轮次")
    p_set.add_argument("--completion", type=int, help="总完成度百分比")
    p_set.add_argument("--iface", type=int, help="接口覆盖率百分比")

    args = parser.parse_args()
    if args.command == "status":
        return cmd_status()
    if args.command == "advance":
        return cmd_advance()
    if args.command == "set":
        return cmd_set(args)
    parser.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
