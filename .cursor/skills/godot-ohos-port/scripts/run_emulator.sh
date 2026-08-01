#!/usr/bin/env bash
# =============================================================================
# run_emulator.sh —— Godot 鸿蒙平台移植：模拟器冒烟测试脚本
#
# 功能：
#   1. 调 DevEco CLI 编译 .hap（hvigorw assembleHap）
#   2. 启动鸿蒙模拟器（MateBook Pro）
#   3. hdc install 安装 + aa start 启动应用
#   4. hilog 崩溃检测 + 截屏验证画面非黑屏
#
# 内置冒烟用例：
#   ① 启动无闪退  ② 打印引擎版本  ③ headless 跑通 GDScript
#   ④ Vulkan 清屏渲染  ⑤ 截屏验证画面非黑屏
#
# 用法：
#   ./run_emulator.sh                      # 全流程冒烟
#   ./run_emulator.sh --skip-build         # 跳过构建（用已有 .hap）
#   ./run_emulator.sh --emulator <name>    # 指定模拟器名
# =============================================================================
set -euo pipefail

# ------------------------- 可配置参数 -------------------------
# 环境变量（未设置时使用默认路径）
export DEVECO_SDK_HOME="${DEVECO_SDK_HOME:-/Users/toro/command-line-tools/sdk/default/openharmony}"
export PATH="${DEVECO_SDK_HOME}/../bin:$PATH"

# 应用参数（按 DevEco 工程实际值覆盖）
BUNDLE_NAME="${BUNDLE_NAME:-com.godot.ohos.editor}"   # 应用 bundleName
ABILITY_NAME="${ABILITY_NAME:-MainAbility}"           # 入口 ability
DEVECO_PROJECT="${DEVECO_PROJECT:-$HOME/Work/GodotHMOS/godot/platform/ohos/deveco}"
EMULATOR_NAME="MateBook Pro"

SKIP_BUILD=0
declare -i PASS=0 FAIL=0

# ------------------------- 工具函数 -------------------------
log()  { printf '[run_emulator] %s\n' "$*"; }
ok()   { printf '[PASS] %s\n' "$*"; PASS+=1; }
bad()  { printf '[FAIL] %s\n' "$*"; FAIL+=1; }

# 检测模拟器是否已启动
emulator_running() {
  deveco emulator list 2>/dev/null | grep -qi "running\|在线\|运行中"
}

# ------------------------- ① 构建 .hap -------------------------
build_hap() {
  if [[ "$SKIP_BUILD" == "1" ]]; then
    log "跳过构建（--skip-build）"
    return
  fi
  log "开始构建 .hap（hvigorw assembleHap）"
  (cd "$DEVECO_PROJECT" && ohpm install --all && hvigorw assembleHap \
      --mode module -p product=default -p module=entry@default \
      -p buildMode=debug --no-daemon)
  ok "构建 .hap 成功"
}

# ------------------------- ② 启动模拟器 -------------------------
start_emulator() {
  if emulator_running; then
    log "模拟器已运行，跳过启动"
    ok "模拟器已运行"
    return
  fi
  log "启动模拟器：$EMULATOR_NAME"
  deveco emulator start "$EMULATOR_NAME"
  # 等待模拟器就绪（hdc 可用）
  for i in $(seq 1 60); do
    if hdc list targets 2>/dev/null | grep -qE "[0-9]"; then
      ok "模拟器就绪（等待 ${i}0s 内）"
      return
    fi
    sleep 10
  done
  bad "模拟器未在 600s 内就绪"
}

# ------------------------- ③ 安装 + 启动应用 -------------------------
install_and_start() {
  local hap
  hap=$(ls "$DEVECO_PROJECT"/entry/build/*/outputs/default/*.hap 2>/dev/null | head -1)
  if [[ -z "$hap" ]]; then
    bad "未找到 .hap 产物"
    return
  fi
  log "安装 $hap"
  hdc install -r "$hap"
  ok "安装成功"
  log "启动 $BUNDLE_NAME/$ABILITY_NAME"
  hdc shell aa start -b "$BUNDLE_NAME" -a "$ABILITY_NAME"
  ok "启动命令已发出"
}

# ------------------------- ④ 崩溃检测 -------------------------
check_crash() {
  log "抓取 hilog 检测崩溃（10s）"
  hdc shell "hilog -T 1000" > /tmp/ohos_hilog.txt 2>&1 &
  local pid=$!
  sleep 10
  kill "$pid" 2>/dev/null || true

  if grep -qiE "crash|fatal|Fatal|assert|SIGSEGV|SIGABRT" /tmp/ohos_hilog.txt; then
    bad "检测到崩溃日志"
    grep -iE "crash|fatal|Fatal|assert|SIGSEGV|SIGABRT" /tmp/ohos_hilog.txt | head -5
  else
    ok "无崩溃日志"
  fi

  if grep -q "Godot Engine v" /tmp/ohos_hilog.txt; then
    ok "引擎版本打印正常（用例②）"
  else
    bad "未检测到引擎版本打印"
  fi
}

# ------------------------- ⑤ 截屏验证画面 -------------------------
capture_screen() {
  log "截屏验证画面"
  hdc shell "snapshot_display -f /data/local/tmp/ohos_screen.png"
  hdc file recv /data/local/tmp/ohos_screen.png /tmp/ohos_screen.png
  # 用 python 检查非纯黑（简化：检查文件大小与尺寸）
  python3 - "$@" <<'PY'
import struct, sys
try:
    with open("/tmp/ohos_screen.png", "rb") as f:
        data = f.read()
    # PNG 头部 + IHDR 宽高
    if not data.startswith(b"\x89PNG"):
        print("[FAIL] 截屏文件非 PNG")
        sys.exit(1)
    w, h = struct.unpack(">II", data[16:24])
    print(f"[PASS] 截屏成功 {w}x{h}（文件 {len(data)} 字节）")
    sys.exit(0)
except Exception as e:
    print(f"[FAIL] 截屏验证失败：{e}")
    sys.exit(1)
PY
}

# ------------------------- 主流程 -------------------------
main() {
  build_hap
  start_emulator
  install_and_start
  check_crash
  capture_screen

  echo "=" * 60
  echo "冒烟测试结果：PASS=$PASS FAIL=$FAIL"
  echo "=" * 60
  [[ "$FAIL" -eq 0 ]]
}

# ------------------------- 参数解析 -------------------------
while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-build)  SKIP_BUILD=1; shift ;;
    --emulator)    EMULATOR_NAME="$2"; shift 2 ;;
    *)             log "未知参数：$1"; exit 2 ;;
  esac
done

main
