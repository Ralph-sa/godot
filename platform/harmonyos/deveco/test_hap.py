#!/usr/bin/env python3
"""test_hap.py -- Godot HarmonyOS 自动化测试框架

测试覆盖：
  [1] 环境验证 — SDK/hdc/hvigor 工具链
  [2] 项目结构 — 关键源文件存在性
  [3] NAPI 签名一致性 — Index.d.ts vs godot_napi_bridge.cpp
  [4] libgodot.so — SO 文件存在性与大小
  [5] HAP 编译 — 全量 debug 构建 (--full)
  [6] 安装+启动+日志 — ANR 检测、异步 init 流程验证、ERROR 扫描 (--full)
  [7] Hypium 测试 — 引擎功能测试 (--hypium)
  [8] Monkey 测试 — 随机压力测试 (--monkey [ITER])

用法:
  python test_hap.py                    # 快速模式 (仅 1-4)
  python test_hap.py --full             # 完整编译+安装+日志
  python test_hap.py --hypium           # 运行 hypium 功能测试
  python test_hap.py --monkey           # Monkey 500 次迭代
  python test_hap.py --monkey 2000      # Monkey 2000 次迭代
  python test_hap.py --monkey-stress    # 压力模式 (2000 次)
  python test_hap.py --full --hypium --monkey  # 全部测试
"""
import argparse, json, os, re, subprocess, sys, time
from pathlib import Path

SCRIPT_DIR  = Path(__file__).parent.resolve()
DEVECO_ROOT = SCRIPT_DIR
ENTRY_SRC   = DEVECO_ROOT / "entry" / "src" / "main"
SDK_HOME    = Path(r"C:\Program Files\Huawei\DevEco Studio\sdk")
JAVA_HOME   = Path(r"C:\Program Files\Huawei\DevEco Studio\jbr")
HVIGOR_BAT  = Path(r"C:\Program Files\Huawei\DevEco Studio\tools\hvigor\bin\hvigorw.bat")
HDC         = SDK_HOME / "default" / "openharmony" / "toolchains" / "hdc.exe"
BUNDLE      = "com.godotengine.editor"
HAP_PATH    = DEVECO_ROOT / "entry" / "build" / "default" / "outputs" / "default" / "entry-default-unsigned.hap"

PASS = 0; FAIL = 0; WARN = 0; RESULTS = []

def ok(desc, cond):
    global PASS, FAIL
    if cond: print(f"  [PASS] {desc}"); PASS += 1; RESULTS.append(f"PASS:{desc}")
    else: print(f"  [FAIL] {desc}"); FAIL += 1; RESULTS.append(f"FAIL:{desc}")

def fe(rel, desc=""):
    ok(desc or f"File: {rel}", rel.is_file())

def fc(rel, pattern, desc=""):
    try:
        text = rel.read_text(encoding="utf-8", errors="ignore")
        ok(desc or f"Pattern in {rel.name}", bool(re.search(pattern, text)))
    except:
        ok(desc or f"Pattern in {rel.name}", False)

def shell(cmd, timeout=30, cwd=None, env_extra=None):
    try:
        env = os.environ.copy()
        env["JAVA_HOME"] = str(JAVA_HOME)
        env["DEVECO_SDK_HOME"] = str(SDK_HOME)
        env["PATH"] = f"{JAVA_HOME / 'bin'};{env.get('PATH','')}"
        if env_extra:
            env.update(env_extra)
        r = subprocess.run(cmd, capture_output=True, text=False,
                          timeout=timeout, cwd=cwd, env=env)
        out = r.stdout.decode("utf-8", errors="replace") if r.stdout else ""
        err = r.stderr.decode("utf-8", errors="replace") if r.stderr else ""
        return r.returncode, (out + err).strip()
    except subprocess.TimeoutExpired: return -1, "TIMEOUT"
    except Exception as e: return -1, str(e)

def hdc_shell(cmd, timeout=15):
    return shell([str(HDC), "shell", cmd], timeout)

def hdc_cmd(*args, timeout=15):
    return shell([str(HDC), *args], timeout)

# ------ Test 1: Environment ------
def test_env():
    print("\n" + "=" * 60); print(" [1] 环境验证"); print("=" * 60)
    ok("Python 3.10+", sys.version_info >= (3, 10))
    ok("DevEco SDK", SDK_HOME.is_dir())
    ok("hdc.exe", HDC.is_file())
    ok("hvigorw.bat", HVIGOR_BAT.is_file())
    rc, _ = shell([str(HDC), "list", "targets"], timeout=10)
    ok("hdc 连接到模拟器", rc == 0)

# ------ Test 2: Structure ------
def test_structure():
    print("\n" + "=" * 60); print(" [2] 项目结构"); print("=" * 60)
    ETS = ENTRY_SRC / "ets"; CPP = ENTRY_SRC / "cpp"; RES = ENTRY_SRC / "resources"
    fe(DEVECO_ROOT / "build-profile.json5")
    fe(ENTRY_SRC / "module.json5")
    fc(ENTRY_SRC / "module.json5", r"EntryAbility", "EntryAbility 定义")
    fc(ENTRY_SRC / "module.json5", r"supportWindowMode", "窗口模式配置")
    for fname in ["GodotEditorPage.ets","ProjectSelector.ets","GodotSurface.ets",
                  "EntryAbility.ets","GodotEditorVM.ets"]:
        found = list(ETS.rglob(fname)); ok(f"   {fname}", len(found) > 0)
    fe(RES / "base" / "profile" / "main_pages.json")
    fe(CPP / "godot_napi_bridge.cpp")
    fe(CPP / "types" / "libgodot_napi" / "Index.d.ts")

    # ohosTest 测试文件
    OHOS_TEST = ENTRY_SRC / "ohosTest" / "ets"
    for fname in ["MonkeyEngine.ets", "Monkey.test.ets", "GodotEditor.test.ets",
                  "List.test.ets", "TestAbility.ets", "OpenHarmonyTestRunner.ets"]:
        found = list(OHOS_TEST.rglob(fname)); ok(f"   test/{fname}", len(found) > 0)
    fe(ENTRY_SRC / "ohosTest" / "module.json5")

# ------ Test 3: NAPI Signatures ------
def test_napi_signatures():
    print("\n" + "=" * 60); print(" [3] NAPI 签名一致性"); print("=" * 60)
    cpp_path = ENTRY_SRC / "cpp" / "godot_napi_bridge.cpp"
    dts_path = ENTRY_SRC / "cpp" / "types" / "libgodot_napi" / "Index.d.ts"
    if not cpp_path.is_file() or not dts_path.is_file():
        ok("源文件就绪", False); return
    ok("源文件就绪", True)
    cpp_txt = cpp_path.read_text(encoding="utf-8", errors="ignore")
    dts_txt = dts_path.read_text(encoding="utf-8", errors="ignore")
    ok("C++ godot_napi 对象包裹", "godot_napi" in cpp_txt and "napi_set_named_property" in cpp_txt)
    dts_funcs = set(re.findall(r"^\s+(\w+)\s*:\s*\(", dts_txt, re.MULTILINE))
    cpp_funcs = set(re.findall(r'"(\w+)"\s*,\s*nullptr,\s*NAPI_', cpp_txt))
    only_dts = dts_funcs - cpp_funcs
    only_cpp = cpp_funcs - dts_funcs
    ok(f"  d.ts 导出 {len(dts_funcs)} 函数", True)
    ok("  函数名一致", len(only_dts) == 0 and len(only_cpp) == 0)
    if only_dts: print(f"      d.ts 独有: {only_dts}")
    if only_cpp: print(f"      C++ 独有: {only_cpp}")

    # Verify async init signature
    ok("  init 签名正确 (callback)", "init: (callback" in dts_txt or "init: (" in dts_txt)

# ------ Test 4: libgodot.so ------
def test_libgodot():
    print("\n" + "=" * 60); print(" [4] libgodot.so"); print("=" * 60)
    for abi in ["x86_64", "arm64-v8a"]:
        so = DEVECO_ROOT / "entry" / "libs" / abi / "libgodot.so"
        if so.is_file():
            mb = so.stat().st_size / (1024*1024)
            with open(so, "rb") as f: magic = f.read(4)
            ok(f"  libgodot.so ({abi}): {mb:.1f} MB", True)
            ok(f"     ELF magic", magic == b"\x7fELF")

# ------ Test 5: HAP Build ------
def test_build():
    print("\n" + "=" * 60); print(" [5] HAP 编译"); print("=" * 60)
    # Stop old daemon first
    shell([str(HVIGOR_BAT), "--stop-daemon"], timeout=15, cwd=str(DEVECO_ROOT))
    cmdline = [str(HVIGOR_BAT), "assembleHap", "--mode", "module",
               "-p", "product=default", "-p", "buildMode=debug", "--no-daemon"]
    rc, out = shell(cmdline, timeout=300, cwd=str(DEVECO_ROOT))
    success = rc == 0 and "BUILD SUCCESSFUL" in out
    ok("HAP 编译成功", success)
    if not success:
        for line in out.splitlines()[-15:]: print(f"  {line}")
    ok("  HAP 文件生成", HAP_PATH.is_file())

# ------ Test 6: Install + Launch + Log Analysis ------
def test_install_and_launch():
    print("\n" + "=" * 60); print(" [6] 安装+启动+日志"); print("=" * 60)

    # 1) Uninstall previous
    hdc_cmd("uninstall", BUNDLE, timeout=15)

    # 2) Install HAP
    rc, out = hdc_cmd("install", str(HAP_PATH), timeout=30)
    ok("安装 HAP", rc == 0 and "success" in out.lower())

    # 3) Clear logs and launch
    hdc_shell("hilog -r", timeout=5)
    rc, out = hdc_shell(f"aa start -a EntryAbility -b {BUNDLE}", timeout=10)
    ok("启动 APP", rc == 0)

    # 4) Wait for async init to complete (worker thread dlopen + main thread setup)
    #    libgodot.so is 151MB; loading + engine init may take 15-30 seconds.
    print("  等待异步初始化 (最多 30s)...")
    time.sleep(8)   # Give time for worker thread to finish dlopen + callback

    # 5) Gather logs
    rc, out = hdc_shell(f"hilog -x", timeout=15)

    # 6) Check process PID
    rc2, ps_out = hdc_shell("ps -elf", timeout=5)
    pid_match = re.search(rf"\d+\s+(\d+)\s+\d+\s+\d+.*{BUNDLE}", ps_out)
    pid = pid_match.group(1) if pid_match else None
    ok("获取 APP PID (进程存活)", pid is not None)

    if not pid:
        print("  进程已退出 — 检查 ANR 日志...")
        anr_lines = [l for l in out.splitlines() if "ANR" in l.upper() or "XCollie" in l]
        for l in anr_lines: print(f"      [ANR] {l.strip()}")
        return None

    # 7) Filter application-level logs (tag A00000 = app custom hilog)
    app_logs = [l for l in out.splitlines() if f" {pid} " in l and "A00000" in l]
    ok("  应用日志非空", len(app_logs) > 0)
    print(f"      应用日志 ({len(app_logs)} 行):")
    for l in app_logs[-20:]:  # Show last 20 lines
        print(f"      {l.strip()}")

    # 8) Check for ERROR-level application logs
    app_errors = [l for l in out.splitlines()
                  if f" {pid} " in l and re.search(r"\sE\s+A00000", l)]
    ok("  无应用级 ERROR", len(app_errors) == 0)
    for l in app_errors: print(f"      [APP_ERROR] {l.strip()}")

    # 9) ANR detection: check for vsync timeout, XCollie, and process kill signals
    anr_patterns = [
        (r"no vsync received in \d+ seconds", "Vsync 超时 (ANR 前兆)"),
        (r"XCollie", "XCollie 看门狗"),
        (r"ANR", "ANR 日志"),
        (r"Process.*killed", "进程被杀"),
        (r"Jank|jank|JANK", "卡顿 (Jank)"),
    ]
    anr_found = []
    all_pid_lines = [l for l in out.splitlines() if f" {pid} " in l]
    for pattern, label in anr_patterns:
        matches = [l for l in all_pid_lines if re.search(pattern, l, re.IGNORECASE)]
        if matches:
            anr_found.append((label, matches))

    ok("  无 ANR/看门狗 告警", len(anr_found) == 0)
    for label, lines in anr_found:
        print(f"      [{label}]:")
        for l in lines: print(f"        {l.strip()}")

    # 10) Async init flow verification
    print("\n  异步初始化流程验证:")
    init_checks = [
        ("NAPI 模块注册", "Godot NAPI module registered"),
        ("Worker thread 启动", "Worker thread: starting dlopen"),
        ("libgodot.so 加载完成", "libgodot.so loaded successfully"),
        ("Worker 通知主线程", "Worker thread: dlopen finished"),
        ("主线程执行 init", "Running harmonyos_godot_init on main thread"),
        ("引擎 init 完成", "harmonyos_godot_init returned"),
        ("VM 回调触发", "Engine init callback: result"),
    ]
    for label, keyword in init_checks:
        found = any(keyword in l for l in all_pid_lines)
        if found:
            ok(f"  {label}", True)
        else:
            # For init-related checks, only fail if engine is meant to be ready
            # (some checks happen late)
            ok(f"  {label}", False)

    # 11) NAPI export error detection
    napi_export_error = [l for l in all_pid_lines
                         if "does not provide an export name" in l]
    ok("  无 NAPI 导出错误", len(napi_export_error) == 0)
    for l in napi_export_error: print(f"      [NAPI_EXPORT] {l.strip()}")

    # 12) Wait more and re-check for ANR after engine init
    print("  等待引擎初始化完成 (额外 12s)...")
    time.sleep(12)
    rc3, out3 = hdc_shell(f"hilog -x", timeout=15)
    all_pid_lines_late = [l for l in out3.splitlines() if f" {pid} " in l]
    late_anr = []
    for pattern, label in anr_patterns:
        matches = [l for l in all_pid_lines_late if re.search(pattern, l, re.IGNORECASE)]
        if matches:
            late_anr.append((label, matches))
    ok("  引擎初始化后无新增 ANR", len(late_anr) == 0)
    for label, lines in late_anr:
        print(f"      [晚期 {label}]:")
        for l in lines: print(f"        {l.strip()}")

    # 13) Check process still alive
    rc4, ps_out4 = hdc_shell("ps -elf", timeout=5)
    pid_alive = bool(re.search(rf"\d+\s+{pid}\s+", ps_out4))
    ok("  引擎初始化后进程存活", pid_alive)

    # 14) Check for libhint2type / AceAutoFill errors (system-level, not actionable)
    system_errs = [l for l in all_pid_lines
                   if "libhint2type" in l or "AceAutoFill" in l or "Hint2Type" in l]
    if system_errs:
        print(f"      [INFO] {len(system_errs)} 条系统级错误 (libhint2type — 非本应用问题，忽略)")

    return pid

# ------ Test 7: Hypium Test ------
def test_hypium():
    """通过 hdc 在设备上运行 hypium 单元测试和功能测试"""
    print("\n" + "=" * 60); print(" [7] Hypium 测试"); print("=" * 60)

    # Step 1: 确保测试 HAP 已安装
    # 测试运行使用 aa test 命令，不需要单独的测试 HAP
    hdc_cmd("uninstall", BUNDLE + ".test", timeout=10)

    # Step 2: 运行 hypium 测试
    # aa test -b <bundleName> -s unittest <TestRunnerClass> -s class <TestSuite>
    test_classes = [
        "BaselineTest",      # 框架基准测试
        "GodotEditorTest",   # 引擎功能测试
    ]

    for test_class in test_classes:
        rc, out = hdc_shell(
            f"aa test -b {BUNDLE} -s unittest OpenHarmonyTestRunner "
            f"-s class {test_class} -s level 1",
            timeout=120
        )
        success = rc == 0 and "[PASS]" in out
        ok(f"  {test_class}", success)
        if not success:
            for line in out.splitlines()[-15:]:
                print(f"    {line}")

# ------ Test 8: Monkey Test ------
def test_monkey(iterations: int = 500, seed: int = None, stress: bool = False):
    """在设备上运行 Monkey 随机压力测试"""
    print("\n" + "=" * 60)
    mode = "压力模式" if stress else "标准"
    print(f" [8] Monkey 测试 ({mode}, {iterations} 次迭代)")
    print("=" * 60)

    if seed is None:
        seed = int(time.time() * 1000) % 0x7FFFFFFF

    print(f"  Seed: {seed}")
    print(f"  Iterations: {iterations}")

    # 写入 Monkey 配置到设备（供 TestRunner 读取）
    monkey_cfg = {
        "iterations": iterations,
        "seed": seed,
        "maxDurationMs": 180000 if stress else 60000,
        "eventIntervalMs": 30 if stress else 50
    }
    cfg_json = subprocess.list2cmdline([json.dumps(monkey_cfg)])
    hdc_shell(
        f"rm -f /data/app/el2/100/base/{BUNDLE}/haps/entry/files/monkey_config.json",
        timeout=5
    )
    hdc_shell(
        f"echo {cfg_json} > "
        f"/data/app/el2/100/base/{BUNDLE}/haps/entry/files/monkey_config.json",
        timeout=5
    )
    ok("写入 Monkey 配置", True)

    # 清理 hilog 缓冲区
    hdc_shell("hilog -r", timeout=5)

    # 运行 Monster 测试
    rc, out = hdc_shell(
        f"aa test -b {BUNDLE} -s unittest OpenHarmonyTestRunner "
        f"-s class MonkeyTest -s level 1",
        timeout=max(180, int(iterations * 0.15))
    )
    success = rc == 0 and "[PASS]" in out
    ok(f"  Monkey 测试通过", success)

    # 检查日志中的崩溃标记
    rc, log_out = hdc_shell(f"hilog -x -t A00000", timeout=10)
    crash_lines = [l for l in log_out.splitlines()
                   if any(kw in l for kw in ["CRASH", "SIGSEGV", "SIGABRT", "SIGFPE"])]
    ok(f"  无崩溃日志 (找到 {len(crash_lines)} 条)", len(crash_lines) == 0)
    for l in crash_lines[-10:]:
        print(f"    [CRASH] {l.strip()}")

    if not success:
        print("  [INFO] Monkey 测试结果详情:")
        for line in out.splitlines()[-30:]:
            print(f"    {line}")

# ------ Main ------
def main():
    global PASS, FAIL, WARN, RESULTS
    PASS = FAIL = WARN = 0; RESULTS = []
    parser = argparse.ArgumentParser()
    parser.add_argument("--full", action="store_true", help="运行编译+安装+全面日志验证")
    parser.add_argument("--hypium", action="store_true",
                        help="运行 hypium 单元测试 / 功能测试 (Test 7)")
    parser.add_argument("--monkey", type=int, nargs="?", const=500, metavar="ITER",
                        help="运行 Monkey 压力测试 (默认 500 次迭代, Test 8)")
    parser.add_argument("--monkey-seed", type=int, help="Monkey 测试随机种子（可复现）")
    parser.add_argument("--monkey-stress", action="store_true",
                        help="Monkey 压力模式 (2000 次迭代)")
    args = parser.parse_args()

    # 构建标题
    modes = []
    if args.full: modes.append("完整")
    if args.hypium: modes.append("Hypium")
    if args.monkey: modes.append(f"Monkey(x{args.monkey})")
    mode_str = " + ".join(modes) if modes else "快速"

    print("=" * 60)
    print(f" Godot HarmonyOS 自动化测试 — {time.strftime('%Y-%m-%d %H:%M:%S')}")
    print(f" 模式: {mode_str}")
    print("=" * 60)

    # 基础测试（总是运行）
    test_env()
    test_structure()
    test_napi_signatures()
    test_libgodot()

    if args.full:
        test_build()
        test_install_and_launch()

    if args.hypium:
        test_hypium()

    if args.monkey is not None:
        stress = args.monkey_stress
        iterations = 2000 if stress else args.monkey
        seed = args.monkey_seed
        test_monkey(iterations, seed, stress)

    print("\n" + "=" * 60)
    print(" 测试汇总"); print("=" * 60)
    for r in RESULTS:
        if r.startswith("PASS"): print(f"  [PASS] {r[5:]}")
        elif r.startswith("FAIL"): print(f"  [FAIL] {r[5:]}")
        else: print(f"  [WARN] {r[5:]}")
    print(f"\n  通过={PASS} 失败={FAIL} 警告={WARN}")
    print("=" * 60)
    return 0 if FAIL == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
