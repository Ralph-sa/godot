#!/bin/bash
# =========================================================================
# test_build_godot.sh - Godot Cross-Compilation Quick Test
# =========================================================================
# Run: bash test_build_godot.sh [--clean]
# =========================================================================

set -euo pipefail
GREEN='\033[0;32m'; RED='\033[0;31m'; NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GODOT_SRC="$SCRIPT_DIR/godot_src"
ENTRY_LIBS="$SCRIPT_DIR/entry/libs/arm64-v8a"
SDK="C:/Users/happyelements/AppData/Local/OpenHarmony/Sdk/20"

export OHOS_SDK_HOME="$SDK"
export PATH="$PATH:/c/Users/happyelements/AppData/Local/Python/pythoncore-3.14-64/Scripts"

echo "=============================================="
echo " Godot Cross-Compilation Test"
echo "=============================================="

# Clean if requested
if [ "${1:-}" = "--clean" ]; then
    echo "[1/3] Cleaning..."
    cd "$GODOT_SRC"
    scons platform=harmonyos -c 2>&1 | tail -2
else
    echo "[1/3] Skipping clean"
fi

# Build
echo "[2/3] Building libgodot.so..."
cd "$GODOT_SRC"
START=$(date +%s)

if scons platform=harmonyos target=editor opengl3=no -j6 2>&1; then
    DURATION=$(( $(date +%s) - START ))
    echo -e "${GREEN}PASS${NC} Build succeeded in ${DURATION}s"
else
    echo -e "${RED}FAIL${NC} Build failed!"
    exit 1
fi

# Verify & copy
GODOT_SO="$GODOT_SRC/bin/libgodot.harmonyos.editor.arm64.so"
echo "[3/3] Verifying..."
file "$GODOT_SO"
mkdir -p "$ENTRY_LIBS"
cp "$GODOT_SO" "$ENTRY_LIBS/libgodot.so"
ls -lh "$ENTRY_LIBS/libgodot.so"

echo "=============================================="
echo -e " ${GREEN}BUILD TEST PASSED${NC}"
echo "=============================================="
