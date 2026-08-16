#!/usr/bin/env bash
# =============================================================================
# build_hap.sh —— Godot HarmonyOS DevEco 工程一键构建脚本
#
# 功能：使用 DevEco Studio 6.1.1（HarmonyOS 6.1.1 / API 24）自带的 hvigor 工具链编译 .hap 安装包。
#       ArkTS 层（Index.ets）+ 原生库（libgodot.so 预编译产物）一并打包。
#
# 前置条件：
#   1. 已安装 DevEco Studio 6.1.1（本脚本自动探测其内置 SDK/工具链/JRE）；
#   2. GODOT_SOURCE_DIR 指向 Godot 源码根目录（或在本脚本同级的 deveco 目录运行）；
#   3. 引擎原生库已通过 SCons 交叉编译为 entry/libs/arm64-v8a/libgodot.so。
#
# 产物：
#   entry/build/default/outputs/default/entry-default-unsigned.hap
#
# 用法：
#   ./build_hap.sh                    # 全量构建（debug 模式，未签名）
#   ./build_hap.sh --clean            # 清理后构建
#   ./build_hap.sh --release          # release 模式（需先配置签名）
# =============================================================================
set -euo pipefail

# ------------------------- 环境探测（DevEco Studio 6.1.1） -------------------------
DEVECO_ROOT="/Applications/DevEco-Studio.app/Contents"

# SDK 根目录（hvigor 需要 DEVECO_SDK_HOME / OHOS_BASE_SDK_HOME 指向含
# default/sdk-pkg.json + default/openharmony/<组件> 的 SDK 根，即 sdk 目录）
export DEVECO_SDK_HOME="${DEVECO_SDK_HOME:-${DEVECO_ROOT}/sdk}"
export OHOS_BASE_SDK_HOME="${OHOS_BASE_SDK_HOME:-${DEVECO_ROOT}/sdk}"

# 内置 JRE（PackageHap 阶段签名/打包需要 Java）
export JAVA_HOME="${DEVECO_ROOT}/jbr/Contents/Home"
export PATH="${JAVA_HOME}/bin:${PATH}"

# hvigor 及其插件依赖（避免项目内 node_modules 冲突导致双实例问题）
export NODE_PATH="${DEVECO_ROOT}/tools/hvigor/hvigor/node_modules:${DEVECO_ROOT}/tools/hvigor/hvigor-ohos-plugin/node_modules"

HVIGORW="${DEVECO_ROOT}/tools/hvigor/bin/hvigorw.js"
NODE="${DEVECO_ROOT}/tools/node/bin/node"

# ------------------------- 参数解析 -------------------------
BUILD_MODE="debug"
CLEAN_FLAG=""

for arg in "$@"; do
    case "$arg" in
        --clean)    CLEAN_FLAG="--clean" ;;
        --release)  BUILD_MODE="release" ;;
        -h|--help)
            echo "用法: ./build_hap.sh [--clean] [--release]"
            exit 0
            ;;
    esac
done

# ------------------------- 切换到工程目录 -------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

echo "======================================================================"
echo " Godot HarmonyOS HAP 构建"
echo "   工程目录 : ${SCRIPT_DIR}"
echo "   SDK      : ${DEVECO_SDK_HOME}"
echo "   JRE      : ${JAVA_HOME}"
echo "   模式     : ${BUILD_MODE}"
echo "======================================================================"

# ------------------------- 执行构建 -------------------------
if [ -n "${CLEAN_FLAG}" ]; then
    echo ">> 清理旧构建产物 ..."
    rm -rf entry/build hvigor/hvigor-config.json5.lock
fi

echo ">> 启动 hvigor assembleHap（${BUILD_MODE}）..."
"${NODE}" "${HVIGORW}" assembleHap \
    --mode module \
    -p product=default \
    -p module=entry@default \
    -p buildMode="${BUILD_MODE}" \
    --no-daemon

# ------------------------- 输出结果 -------------------------
# 已配置 signingConfig 时产物为 signed.hap（未配置时回落 unsigned.hap）
SIGNED_HAP="entry/build/default/outputs/default/entry-default-signed.hap"
HAP="entry/build/default/outputs/default/entry-default-unsigned.hap"
if [ -f "${SIGNED_HAP}" ]; then
    HAP="${SIGNED_HAP}"
fi
if [ -f "${HAP}" ]; then
    SIZE=$(du -h "${HAP}" | cut -f1)
    echo "======================================================================"
    echo " 构建成功！HAP 产物: ${HAP}（${SIZE}）"
    echo " 提示：该 HAP 未签名。真机/模拟器安装需在 DevEco Studio 中配置"
    echo "       自动签名（File > Project Structure > Signing Configs）。"
    echo "======================================================================"
else
    echo "!! 构建结束但未找到 HAP 产物，请检查上方日志。" >&2
    exit 1
fi
