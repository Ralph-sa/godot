#!/usr/bin/env python3
# GodotHMOS 移植自动化测试（真机/模拟器通用）
# 用法: python3 run_tests.py --device <hdc-target> [--hdc <path>] [--tap "x y"]
import argparse, subprocess, sys, time

CACHE = "/data/app/el2/100/base/com.godot.editor/haps/entry/cache"

def sh(hdc, device, cmd, timeout=60):
    full = [hdc] + (["-t", device] if device else []) + ["shell", cmd]
    try:
        r = subprocess.run(full, capture_output=True, text=True, timeout=timeout)
        return r.stdout
    except Exception as e:
        return "ERROR: " + str(e)

def read_file(hdc, device, path):
    return sh(hdc, device, "cat " + path + " 2>/dev/null")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--device", default="127.0.0.1:5555")
    ap.add_argument("--hdc", default="/Users/toro/HarmonyOS/command-line-tools/sdk/default/openharmony/toolchains/hdc")
    ap.add_argument("--tap", default="1560 980")
    ap.add_argument("--wait", type=int, default=30)
    args = ap.parse_args()
    hdc, dev = args.hdc, args.device
    results = []
    def check(tid, name, ok, detail=""):
        results.append((tid, name, ok, detail))
        print(("[PASS] " if ok else "[FAIL] ") + tid + " " + name + ((" | " + detail) if detail else ""))
    marker = read_file(hdc, dev, CACHE + "/godot_marker.log")
    ok = ("globals->setup OK" in marker and "start: editor=1" in marker and "EditorNode: ctor done" in marker)
    check("T01", "startup chain", ok, "marker " + str(len(marker)) + "B")
    s1 = read_file(hdc, dev, CACHE + "/godot_ds_diag.log")
    time.sleep(4)
    s2 = read_file(hdc, dev, CACHE + "/godot_ds_diag.log")
    def swap_count(s):
        n = 0
        for line in s.splitlines():
            if "swap_buffers: count=" in line:
                try: n = int(line.split("count=")[1].split()[0])
                except ValueError: pass
        return n
    c1, c2 = swap_count(s1), swap_count(s2)
    check("T02", "render loop", c2 > c1 and c2 > 0, "swap %d -> %d" % (c1, c2))
    ds = read_file(hdc, dev, CACHE + "/godot_ds_diag.log")
    eg_ok = ("egl initialize -> 0" in ds and "egl open_display -> 0" in ds and "egl window_create -> 0" in ds)
    check("T05", "EGL chain", eg_ok, "ds " + str(len(ds)) + "B")
    pid = sh(hdc, dev, "pidof com.godot.editor").strip()
    crash = read_file(hdc, dev, CACHE + "/godot_crash.log")
    check("T06", "process alive", bool(pid), "pid=" + pid if pid else "dead")
    sh(hdc, dev, "rm -f " + CACHE + "/godot_input_diag.log")
    sh(hdc, dev, "uitest uiInput click " + args.tap + " >/dev/null 2>&1")
    time.sleep(2)
    inp = read_file(hdc, dev, CACHE + "/godot_input_diag.log")
    check("T04", "input injection", ("injectTouch" in inp or "injectMouse" in inp or "push_touch" in inp), "input " + str(len(inp)) + "B")
    sh(hdc, dev, "uitest screenCap -p /data/local/tmp/t_test.jpeg >/dev/null 2>&1")
    subprocess.run([hdc] + (["-t", dev] if dev else []) + ["file", "recv", "/data/local/tmp/t_test.jpeg", "/tmp/t_test.jpeg"], capture_output=True, timeout=60)
    try:
        from PIL import Image
        img = Image.open("/tmp/t_test.jpeg").convert("RGB")
        w, h = img.size
        px = img.load()
        n = 0; dark = 0; mid = 0
        for y in range(h//4, 3*h//4, 20):
            for x in range(w//4, 3*w//4, 20):
                p = px[x, y]
                n += 1
                if sum(p) < 90: dark += 1
                elif sum(p) < 400: mid += 1
        ok = (dark + mid) / n > 0.3
        check("T03", "window content", ok, "%dx%d dark=%.0f%% mid=%.0f%%" % (w, h, 100*dark/n, 100*mid/n))
    except Exception as e:
        check("T03", "window content", False, str(e))
    fails = [r for r in results if not r[2]]
    print("\n==== %d PASS / %d FAIL ====" % (len(results)-len(fails), len(fails)))
    for r in fails:
        print("  FAIL " + r[0] + " " + r[1] + " | " + r[3])
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
