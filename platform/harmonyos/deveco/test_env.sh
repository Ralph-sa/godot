#!/bin/bash
# =========================================================================
# test_env.sh - Environment Validation Test
# Run: bash test_env.sh
# =========================================================================

# set -euo pipefail  # Use manual error handling for better reporting
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
PASS=0; FAIL=0; WARN=0

# Ensure Python and SCons are findable
export PATH="$PATH:/c/Users/happyelements/AppData/Local/Python/pythoncore-3.14-64/Scripts"

check() {
    local desc="$1"; shift
    if "$@" > /dev/null 2>&1; then
        echo -e "  [${GREEN}PASS${NC}] $desc"
        ((PASS++))
    else
        echo -e "  [${RED}FAIL${NC}] $desc"
        ((FAIL++))
    fi
}

warn_check() {
    local desc="$1"; shift
    if "$@" > /dev/null 2>&1; then
        echo -e "  [${GREEN}PASS${NC}] $desc"
        ((PASS++))
    else
        echo -e "  [${YELLOW}WARN${NC}] $desc"
        ((WARN++))
    fi
}

echo "=============================================="
echo " Godot HarmonyOS - Environment Validation"
echo "=============================================="
echo ""

# ---- 1. Python ----
echo "[1] Python"
PY_VER=$(python --version 2>&1)
echo "    Version: $PY_VER"
check "Python 3.10+" python -c "import sys; assert sys.version_info >= (3,10)"
check "SCons installed" python -c "import SCons; print('SCons', SCons.__version__)"
echo ""

# ---- 2. Node.js / DevEco CLI ----
echo "[2] Node.js & DevEco CLI"
NODE_VER=$(node --version 2>&1)
echo "    Node: $NODE_VER"
check "Node.js 18+" node -e "const v=process.version.slice(1).split('.'); assert(parseInt(v[0])>=18)"
warn_check "DevEco CLI installed" npm list -g @deveco/deveco-cli 2>/dev/null | grep -q deveco
echo ""

# ---- 3. OHOS SDK ----
echo "[3] OHOS SDK"
SDK="C:/Users/happyelements/AppData/Local/OpenHarmony/Sdk/20"
if [ -d "$SDK" ]; then
    echo -e "  [${GREEN}PASS${NC}] SDK directory found"
    ((PASS++))
else
    echo -e "  [${RED}FAIL${NC}] SDK not found at: $SDK"
    ((FAIL++))
fi
check "  Native SDK"          test -d "$SDK/native"
check "  LLVM toolchain"      test -f "$SDK/native/llvm/bin/clang.exe"
check "  Vulkan OHOS headers" test -f "$SDK/native/sysroot/usr/include/vulkan/vulkan_ohos.h"
check "  XComponent headers"  test -f "$SDK/native/sysroot/usr/include/ace/xcomponent/native_interface_xcomponent.h"
check "  hdc tool"            test -f "$SDK/toolchains/hdc.exe"
echo ""

# ---- 4. Project Structure ----
echo "[4] GodotHOS Project"
PROJ="c:/Toro/GodotHOS"
check "build-profile.json5"        test -f "$PROJ/build-profile.json5"
check "hvigorfile.ts"              test -f "$PROJ/hvigorfile.ts"
check "entry/module.json5"         test -f "$PROJ/entry/src/main/module.json5"
check "main_pages.json"            test -f "$PROJ/entry/src/main/resources/base/profile/main_pages.json"
check "GodotSurface.ets"           test -f "$PROJ/entry/src/main/ets/components/GodotSurface.ets"
check "EntryAbility.ets"           test -f "$PROJ/entry/src/main/ets/entryability/EntryAbility.ets"
check "GodotEditorPage.ets"        test -f "$PROJ/entry/src/main/ets/pages/GodotEditorPage.ets"
check "NAPI bridge"                test -f "$PROJ/entry/src/main/cpp/godot_napi_bridge.cpp"
check "CMakeLists.txt"             test -f "$PROJ/entry/src/main/cpp/CMakeLists.txt"
echo ""

# ---- 5. libgodot.so ----
echo "[5] libgodot.so"
GODOT_SO="$PROJ/entry/libs/arm64-v8a/libgodot.so"
check "File exists" test -f "$GODOT_SO"
SZ=$(ls -lh "$GODOT_SO" 2>/dev/null | awk '{print $5}')
echo "    Size: $SZ"
echo ""

# ---- 6. Godot Platform Sources ----
echo "[6] Godot Platform"
HOS="$PROJ/godot_src/platform/harmonyos"
check "os_harmonyos.cpp"           test -f "$HOS/os_harmonyos.cpp"
check "display_server_harmonyos.cpp" test -f "$HOS/display_server_harmonyos.cpp"
check "vulkan_ohos_surface.h"      test -f "$HOS/vulkan_ohos_surface.h"
check "harmonyos_main.cpp"         test -f "$HOS/harmonyos_main.cpp"
check "harmonyos_input.cpp"        test -f "$HOS/harmonyos_input.cpp"
check "SCsub"                      test -f "$HOS/SCsub"
check "detect.py"                  test -f "$HOS/detect.py"
echo ""

# ---- Summary ----
echo "=============================================="
echo -e " ${GREEN}PASS:${NC} $PASS  ${RED}FAIL:${NC} $FAIL  ${YELLOW}WARN:${NC} $WARN"
echo "=============================================="
if [ "$FAIL" -eq 0 ]; then
    echo -e " ${GREEN}ALL CHECKS PASSED${NC}"
    exit 0
else
    echo -e " ${RED}$FAIL CHECK(S) FAILED${NC}"
    exit 1
fi
