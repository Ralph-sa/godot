#!/usr/bin/env python3
"""test_structure.py - Project Structure and Integrity Validator

Validates the complete GodotHOS project structure.
Usage: python test_structure.py
"""

import os, sys, re, json
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.resolve()
WORKSPACE_ROOT = PROJECT_ROOT.parents[3]
BUILD_SCRIPT = WORKSPACE_ROOT / ".cursor/skills/harmonyos-tools/scripts/build_project.sh"
PASS = 0; FAIL = 0

def check(desc, condition):
    global PASS, FAIL
    if condition:
        print(f"  [PASS] {desc}"); PASS += 1
    else:
        print(f"  [FAIL] {desc}"); FAIL += 1

def fe(relpath, desc=None):
    check(desc or f"File: {relpath}", (PROJECT_ROOT / relpath).is_file())

def fc(relpath, pattern, desc=None):
    p = PROJECT_ROOT / relpath
    if p.is_file():
        check(desc or f"Pattern in {relpath}", bool(re.search(pattern, p.read_text(encoding="utf-8", errors="ignore"))))
    else:
        check(desc or f"Pattern in {relpath}", False)

def main():
    print("=" * 60)
    print(" GodotHOS Project Structure Validator")
    print("=" * 60); print()

    print("[1] Root Config")
    fe("build-profile.json5"); fe("hvigorfile.ts"); fe("AppScope/app.json5")
    fc("AppScope/app.json5", r"com\.godotengine\.editor", "Bundle name")
    print()

    print("[2] Entry Module Config")
    fe("entry/build-profile.json5"); fe("entry/hvigorfile.ts")
    fe("entry/src/main/module.json5")
    fc("entry/src/main/module.json5", r"EntryAbility", "EntryAbility defined")
    fc("entry/src/main/module.json5", r"\"2in1\"", "2in1 device type")
    fc("entry/src/main/module.json5", r"ArkTSPattern", "ArkTSPattern V2")
    fc("entry/build-profile.json5", r"nativeLib", "Native lib configured")
    fc("entry/build-profile.json5",
       r'"abiFilters"\s*:\s*\[\s*"x86_64"\s*,\s*"arm64-v8a"\s*\]',
       "Dual ABI filters: x86_64 + arm64-v8a")
    print()

    print("[3] ArkTS @ComponentV2")
    for f in ["pages/GodotEditorPage.ets", "components/GodotSurface.ets",
              "entryability/EntryAbility.ets", "viewmodel/GodotEditorVM.ets", "model/EditorConfig.ets"]:
        fe(f"entry/src/main/ets/{f}")
    fc("entry/src/main/ets/pages/GodotEditorPage.ets", r"@ComponentV2", "@ComponentV2 in page")
    fc("entry/src/main/ets/components/GodotSurface.ets", r"XComponent", "XComponent in surface")
    fc("entry/src/main/ets/viewmodel/GodotEditorVM.ets", r"@ObservedV2", "@ObservedV2 in VM")
    fc("entry/src/main/ets/viewmodel/GodotEditorVM.ets", r"@Trace", "@Trace in VM")
    print()

    print("[4] NAPI Bridge")
    fe("entry/src/main/cpp/godot_napi_bridge.cpp"); fe("entry/src/main/cpp/CMakeLists.txt")
    fe("entry/src/main/cpp/types/libgodot_napi/Index.d.ts")
    fc("entry/src/main/cpp/godot_napi_bridge.cpp", r"napi_module_register", "NAPI module register")
    fc("entry/src/main/cpp/godot_napi_bridge.cpp", r'dlopen\("libgodot\.so"', "dlopen libgodot.so")
    bridge_text = (PROJECT_ROOT / "entry/src/main/cpp/godot_napi_bridge.cpp").read_text(
        encoding="utf-8", errors="strict")
    check("No ABI-specific libgodot fallback",
          "libgodot.harmonyos.editor.arm64.so" not in bridge_text)
    fc("entry/src/main/cpp/CMakeLists.txt", r"libgodot_napi", "CMake target: libgodot_napi")
    fc("entry/src/main/cpp/CMakeLists.txt", r"-g3", "NAPI debug symbols enabled")
    fc("entry/build-profile.json5", r'"debugSymbol"\s*:\s*\{\s*"strip"\s*:\s*false',
       "Hvigor native symbol stripping disabled")
    fc("entry/src/main/cpp/types/libgodot_napi/Index.d.ts",
       r"loadLibrary\s*:\s*\(projectPath:\s*string,\s*filesDir:\s*string,\s*"
       r"cacheDir:\s*string,\s*tempDir:\s*string,\s*surfaceId:\s*string,\s*"
       r"width:\s*number,\s*height:\s*number,\s*generation:\s*number\)\s*=>\s*number",
       "TS: loadLibrary(projectPath, sandbox roots, surface) declared")
    fe("test_port_contracts.py", "Focused HarmonyOS port contract tests")
    print()

    print("[5] Godot Platform Layer")
    platform = "../../../platform/harmonyos"
    for f in ["os_harmonyos.cpp", "display_server_harmonyos.cpp", "harmonyos_native_window.cpp",
              "harmonyos_input.cpp", "harmonyos_audio.cpp", "harmonyos_main.cpp",
              "rendering_context_driver_vulkan_harmonyos.cpp", "vulkan_ohos_surface.h",
              "SCsub", "detect.py"]:
        fe(f"{platform}/{f}")
    fc(f"{platform}/display_server_harmonyos.h", r"class DisplayServerHarmonyOS", "DisplayServer class")
    fc(f"{platform}/rendering_context_driver_vulkan_harmonyos.cpp", r"VK_OHOS_SURFACE_EXTENSION_NAME", "VK_OHOS_surface ext")
    fc(f"{platform}/rendering_context_driver_vulkan_harmonyos.cpp", r"vkCreateSurfaceOHOS", "Dynamic vkCreateSurfaceOHOS")
    fc(f"{platform}/harmonyos_input.cpp", r"ohos_key_to_godot", "Key mapping function")
    fc(f"{platform}/harmonyos_main.cpp", r"Main::setup", "Main::setup call")
    fc(f"{platform}/harmonyos_main.cpp", r"--editor", "Editor flag")
    fc(f"{platform}/detect.py", r"HARMONYOS_ENABLED", "HARMONYOS_ENABLED define")
    fc(f"{platform}/detect.py", r"VULKAN_ENABLED", "VULKAN_ENABLED define")
    print()

    print("[6] Configured ABI Artifacts")
    for abi in ("x86_64", "arm64-v8a"):
        so = PROJECT_ROOT / f"entry/libs/{abi}/libgodot.so"
        if so.is_file():
            mb = so.stat().st_size / (1024 * 1024)
            check(f"{abi} libgodot.so ({mb:.1f} MB)", True)
            check(f"{abi} size >= 5 MB", so.stat().st_size >= 5 * 1024 * 1024)
            with open(so, "rb") as f:
                magic = f.read(4)
            check(f"{abi} valid ELF magic", magic == b"\x7fELF")
        else:
            print(f"  [INFO] {abi} libgodot.so not built during static validation")
    print()

    print("[7] Resources & Scripts")
    fe("entry/src/main/resources/base/profile/main_pages.json")
    fe("entry/src/main/resources/base/element/string.json")
    fe("build_harmonyos.sh"); fe("test_env.sh")
    fc("entry/src/main/resources/base/profile/main_pages.json", r"GodotEditorPage", "Page in main_pages.json")
    check("Dual ABI build wrapper exists", BUILD_SCRIPT.is_file())
    if BUILD_SCRIPT.is_file():
        build_script = BUILD_SCRIPT.read_text(encoding="utf-8", errors="strict")
        check("--arch all is documented", "--arch <arm64|x86_64|all>" in build_script)
        check("--arch all selects both SCons architectures",
              'TARGET_ARCHES=("x86_64" "arm64")' in build_script)
        check("Single-architecture mode remains available",
              'arm64|x86_64) TARGET_ARCHES=("$ARCH")' in build_script)
        check("arm64 output maps to arm64-v8a",
              'arm64)  libs_arch_dir="arm64-v8a"' in build_script)
        check("x86_64 output maps to x86_64",
              'x86_64) libs_arch_dir="x86_64"' in build_script)
        check("Architectures build sequentially",
              'for target_arch in "${TARGET_ARCHES[@]}"' in build_script)
        check("Each ABI copies ABI-local libgodot.so",
              'cp "$godot_so" "$libs_dir/libgodot.so"' in build_script)
        check("Git Bash restores PROCESSOR_ARCHITECTURE",
              "PROCESSOR_ARCHITECTURE=AMD64" in build_script)
        check("Packaged engine is stripped by default",
              '"$ENGINE_STRIP_TOOL" --strip-all -o "$libs_dir/libgodot.so" "$godot_so"'
              in build_script)
        check("Full engine symbols remain an explicit opt-in",
              "--keep-engine-symbols" in build_script)
        check("HAP packaging runs exactly once",
              build_script.count('./build_hap_only.bat "$BUILD_TYPE"') == 1)
    print()

    print("=" * 60)
    print(f" RESULTS: {PASS} passed, {FAIL} failed")
    print("=" * 60)
    return 0 if FAIL == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
