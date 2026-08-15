#!/usr/bin/env bash
# =============================================================================
# run_verify.sh —— Godot 鸿蒙编辑器 HAP 模拟器验证脚本（第 10 轮修复后）
#
# 前置条件：模拟器已从 DevEco Studio Device Manager 启动（命令行 Emulator
# -start 会因缺失 SN/uuid 文件失败 —— 该文件由 IDE 点击启动时写入 TMPDIR）。
#
# 流程：
#   1. 安装 entry-default-signed.hap（hdc install -r）
#   2. 启动应用（aa start）并抓取 hilog
#   3. 截屏验证项目管理器画面
#   4. 拉取引擎诊断文件（cacheDir 下的 godot_engine_diag.log / godot_ds_diag.log）
#   5. 输出故障检查清单
#
# 用法：
#   ./run_verify.sh            # 全流程
# =============================================================================
set -uo pipefail

BUNDLE_NAME="${BUNDLE_NAME:-com.godot.editor}"
ABILITY_NAME="${ABILITY_NAME:-EntryAbility}"
HAP_DIR="$(cd "$(dirname "$0")" && pwd)/entry/build/default/outputs/default"
OUT_DIR="${OUT_DIR:-/tmp/ohos_verify}"
mkdir -p "$OUT_DIR"

export PATH="$HOME/command-line-tools/bin:$HOME/command-line-tools/sdk/default/openharmony/toolchains:$PATH"

log()  { printf '[verify] %s\n' "$*"; }
fail() { printf '[FAIL] %s\n' "$*"; }

# 1. 检查 hdc 设备
log "检查 hdc 设备..."
if ! hdc list targets 2>/dev/null | grep -qE '[0-9]'; then
    fail "未检测到设备。请先在 DevEco Studio Device Manager 启动 MateBook Pro 模拟器"
    exit 1
fi
log "设备在线"

# 2. 安装 HAP
HAP="$HAP_DIR/entry-default-signed.hap"
if [ ! -f "$HAP" ]; then
    fail "未找到 $HAP（先运行 ./build_hap.sh）"
    exit 1
fi
log "安装 $HAP"
hdc install -r "$HAP" || fail "安装失败（签名问题？检查 build-profile.json5 signingConfig）"
log "安装完成"

# 3. 清掉旧的诊断文件（下次启动重新生成）
log "清理旧诊断文件..."
hdc shell rm -f /data/storage/el2/base/haps/entry/cache/godot_engine_diag.log 2>/dev/null
hdc shell rm -f /data/storage/el2/base/haps/entry/cache/godot_ds_diag.log 2>/dev/null

# 4. 启动应用并抓取 hilog（60 秒窗口，覆盖引擎启动）
log "启动应用 $BUNDLE_NAME/$ABILITY_NAME"
hdc shell aa start -b "$BUNDLE_NAME" -a "$ABILITY_NAME" 2>&1 | head -3
log "抓取 hilog 60s（启动窗口）..."
hdc shell "hilog -T 60000" > "$OUT_DIR/hilog_start.txt" 2>&1 &
HILOG_PID=$!

# 5. 阶段截图：15s / 45s 各一张
sleep 15
hdc shell "snapshot_display -f /data/local/tmp/verify_1.png" >/dev/null 2>&1
hdc file recv /data/local/tmp/verify_1.png "$OUT_DIR/screen_15s.png" >/dev/null 2>&1
log "截图1（15s，应显示 Godot 项目管理器）: $OUT_DIR/screen_15s.png"

sleep 30
hdc shell "snapshot_display -f /data/local/tmp/verify_2.png" >/dev/null 2>&1
hdc file recv /data/local/tmp/verify_2.png "$OUT_DIR/screen_45s.png" >/dev/null 2>&1
log "截图2（45s）: $OUT_DIR/screen_45s.png"

wait "$HILOG_PID" 2>/dev/null

# 6. 拉取诊断文件
log "拉取引擎诊断文件..."
for p in \
    "/data/storage/el2/base/haps/entry/cache/godot_engine_diag.log" \
    "/data/storage/el2/base/haps/entry/cache/godot_ds_diag.log" ; do
    base=$(basename "$p")
    hdc file recv "$p" "$OUT_DIR/$base" >/dev/null 2>&1 && log "  得到 $base"
done

# 7. 崩溃检测
log "崩溃/错误检测..."
if grep -qiE "SIGSEGV|SIGABRT|Fatal|crash" "$OUT_DIR/hilog_start.txt"; then
    fail "检测到崩溃信号："
    grep -iE "SIGSEGV|SIGABRT|Fatal" "$OUT_DIR/hilog_start.txt" | head -5
else
    log "无崩溃信号"
fi

# 8. 引擎阶段输出
log "引擎关键日志："
grep -iE "Godot|godot|DisplayServerOHOS|engine_thread" "$OUT_DIR/hilog_start.txt" | grep -v '^$' | head -30

# 9. 诊断文件内容
for f in "$OUT_DIR"/godot_engine_diag.log "$OUT_DIR"/godot_ds_diag.log; do
    if [ -f "$f" ]; then
        log "===== $f ====="
        cat "$f"
    fi
done

log ""
log "人工验证清单："
log "  [ ] 截图1 显示 Godot 项目管理器（说明引擎+Vulkan 渲染链路 OK）"
log "  [ ] 项目管理器中点「新建项目」→ 创建 → 「创建并编辑」"
log "  [ ] 编辑器打开（不再卡住/黑屏）—— 进程内重启生效"
log "  [ ] 卡住时执行：hdc shell 'hilog | grep -i godot' 与诊断文件定位卡点"
log "输出目录：$OUT_DIR"
