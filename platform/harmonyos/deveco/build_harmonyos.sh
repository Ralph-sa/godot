#!/bin/bash
# =========================================================================
# build_harmonyos.sh - Complete Build Pipeline for Godot HarmonyOS Editor
# =========================================================================
# This script builds the entire Godot Editor for HarmonyOS from source,
# generating a deployable HAP package.
#
# Prerequisites:
#   1. DevEco Studio / Command Line Tools installed
#   2. OpenHarmony SDK (API 20+)
#   3. Python 3.10+ with scons installed
#   4. OHOS LLVM toolchain (aarch64-linux-ohos-clang)
#
# Usage:
#   ./build_harmonyos.sh [--release|--debug] [--clean]
# =========================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
GODOT_SRC="$SCRIPT_DIR/../../.."
ENTRY_DIR="$PROJECT_ROOT/entry"
BUILD_TYPE="${1:-debug}"

# OHOS SDK setup
export OHOS_SDK_HOME="${OHOS_SDK_HOME:-$HOME/AppData/Local/OpenHarmony/Sdk/20}"
export OHOS_NDK_HOME="$OHOS_SDK_HOME/native"
export PATH="$OHOS_SDK_HOME/toolchains:$OHOS_NDK_HOME/llvm/bin:$PATH"

echo "=============================================="
echo " Godot HarmonyOS Editor Build Pipeline"
echo "=============================================="
echo " SDK: $OHOS_SDK_HOME"
echo " Build Type: $BUILD_TYPE"
echo "=============================================="

# ============================================
# Step 1: Build libgodot.so via SCons
# ============================================
echo ""
echo "[1/4] Building Godot Engine (libgodot.so)..."
echo "----------------------------------------------"

cd "$GODOT_SRC"

SCONS_TARGET="editor"
SCONS_FLAGS="platform=harmonyos target=$SCONS_TARGET opengl3=no -j$(nproc)"

if [ "${2:-}" = "--clean" ]; then
    echo "Cleaning previous build..."
    scons platform=harmonyos -c
fi

echo "Running: scons $SCONS_FLAGS"
scons $SCONS_FLAGS

# Verify output
GODOT_SO="$GODOT_SRC/bin/libgodot.harmonyos.editor.arm64.so"
if [ ! -f "$GODOT_SO" ]; then
    echo "ERROR: Failed to build libgodot.so"
    exit 1
fi

echo "-> libgodot.so built: $(ls -lh "$GODOT_SO" | awk '{print $5}')"

# ============================================
# Step 2: Copy libgodot.so to native libs
# ============================================
echo ""
echo "[2/4] Copying libgodot.so to HAP libs..."
echo "----------------------------------------------"

LIBS_DIR="$ENTRY_DIR/libs/arm64-v8a"
mkdir -p "$LIBS_DIR"
cp "$GODOT_SO" "$LIBS_DIR/libgodot.so"
echo "-> Copied to: $LIBS_DIR/libgodot.so"

# ============================================
# Step 3: Build HAP via hvigor
# ============================================
echo ""
echo "[3/4] Building HarmonyOS HAP package..."
echo "----------------------------------------------"

cd "$PROJECT_ROOT"

# Check if hvigorw is available
if command -v hvigorw &> /dev/null; then
    echo "Building with hvigorw..."
    hvigorw assembleHap --mode module -p product=default -p buildMode=$BUILD_TYPE
elif [ -f "./hvigorw" ]; then
    echo "Building with local hvigorw..."
    chmod +x ./hvigorw
    ./hvigorw assembleHap --mode module -p product=default -p buildMode=$BUILD_TYPE
elif command -v node &> /dev/null && [ -f "./hvigor/hvigor-wrapper.js" ]; then
    echo "Building with hvigor wrapper..."
    node ./hvigor/hvigor-wrapper.js assembleHap --mode module -p product=default -p buildMode=$BUILD_TYPE
else
    echo "WARNING: hvigorw not found. Skipping HAP build."
    echo "Please build from DevEco Studio or install hvigor CLI."
    echo ""
    echo "Manual build steps:"
    echo "  1. Open $PROJECT_ROOT in DevEco Studio"
    echo "  2. Build -> Build HAP(s)"
    echo "  3. Output: entry/build/default/outputs/default/entry-default-signed.hap"
fi

# ============================================
# Step 4: Summary
# ============================================
echo ""
echo "[4/4] Build Summary"
echo "----------------------------------------------"
echo " libgodot.so: $(ls -lh "$GODOT_SO" | awk '{print $5}')"
echo " NAPI bridge: entry/src/main/cpp/"
echo " ArkTS UI:    entry/src/main/ets/"

HAP_PATH="$ENTRY_DIR/build/default/outputs/default/entry-default-signed.hap"
if [ -f "$HAP_PATH" ]; then
    echo " HAP package: $(ls -lh "$HAP_PATH" | awk '{print $5}')"
    echo ""
    echo "=============================================="
    echo " BUILD SUCCESSFUL"
    echo "=============================================="
    echo " HAP: $HAP_PATH"
else
    echo ""
    echo "=============================================="
    echo " GODOT .SO BUILD SUCCESSFUL"
    echo "=============================================="
    echo " Next step: Build HAP from DevEco Studio"
    echo " or run: hvigorw assembleHap"
fi
