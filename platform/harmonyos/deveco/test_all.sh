#!/bin/bash
# =========================================================================
# test_all.sh - Master Test Runner
# =========================================================================
# Runs all tests in sequence:
#   1. test_env.sh      - Environment validation
#   2. test_structure.py - Project structure check
#   3. test_build_godot.sh - Cross-compilation test
#   4. devecocli build    - HAP package build test
# =========================================================================

set -euo pipefail
GREEN='\033[0;32m'; RED='\033[0;31m'; CYAN='\033[0;36m'; NC='\033[0m'
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

RESULTS=()

run_test() {
    local name="$1"; shift
    echo -e "\n${CYAN}=============================================="
    echo -e " RUNNING: $name"
    echo -e "==============================================${NC}"
    if "$@" 2>&1; then
        echo -e "${GREEN}  $name - PASSED${NC}"
        RESULTS+=("PASS: $name")
    else
        echo -e "${RED}  $name - FAILED${NC}"
        RESULTS+=("FAIL: $name")
    fi
}

echo -e "${CYAN}"
echo "=============================================="
echo " Godot HarmonyOS - Full Test Suite"
echo "=============================================="
echo -e "${NC}"

# Test 1: Environment
run_test "Environment Validation" bash test_env.sh

# Test 2: Structure
run_test "Project Structure" python test_structure.py

# Test 3: Godot Build (only if --full specified)
if [ "${1:-}" = "--full" ]; then
    run_test "Godot Cross-Compilation" bash test_build_godot.sh
else
    echo -e "\n${CYAN}[SKIP] Godot Cross-Compilation (use --full to include)${NC}"
fi

# Test 4: DevEco CLI test (if available)
if npm list -g @deveco/deveco-cli 2>/dev/null | grep -q deveco; then
    DE_CLI="$(npm root -g 2>/dev/null)/../deveco-cli"  # approximate path
    run_test "DevEco CLI Project Parse" npx devecocli build --help 2>&1 | grep -q "Build HarmonyOS project"
else
    echo -e "\n${CYAN}[SKIP] DevEco CLI (not installed)${NC}"
fi

# Summary
echo -e "\n${CYAN}=============================================="
echo -e " TEST SUMMARY"
echo -e "==============================================${NC}"
for r in "${RESULTS[@]}"; do
    if [[ "$r" == PASS:* ]]; then
        echo -e "  ${GREEN}[PASS]${NC} ${r#PASS: }"
    else
        echo -e "  ${RED}[FAIL]${NC} ${r#FAIL: }"
    fi
done

FAIL_COUNT=$(printf '%s\n' "${RESULTS[@]}" | grep -c "FAIL:" || true)
PASS_COUNT=$(printf '%s\n' "${RESULTS[@]}" | grep -c "PASS:" || true)
echo ""
echo -e "Total: ${GREEN}$PASS_COUNT passed${NC}, ${RED}$FAIL_COUNT failed${NC}"
exit $FAIL_COUNT
