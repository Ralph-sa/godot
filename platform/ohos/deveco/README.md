# Godot HarmonyOS DevEco 工程模板（骨架）

本目录提供将 Godot 引擎（平台层 `platform/ohos/`）嵌入鸿蒙应用的 **DevEco Studio 工程骨架**。
编辑器 UI 完全由 Godot 通过 Vulkan 自绘，宿主为一个全屏 `XComponent(SURFACE)`。

## 目录结构

```
deveco/
├── CMakeLists.txt                          # native 层编译配置（libgodot.so）
└── entry/
    └── src/main/
        ├── module.json5                    # 应用模块声明（2in1 设备支持）
        ├── ets/
        │   ├── entryability/EntryAbility.ets  # UIAbility 入口
        │   └── pages/Index.ets             # 主页（全屏 XComponent）
        └── resources/                      # 资源目录（string/color/media/profile）
```

## 使用方法（骨架期）

1. 在 DevEco Studio 26.0.0 中新建空工程（`Empty Ability`，支持 `2in1` 设备类型）；
2. 将本目录 `entry/src/main/ets` 与 `module.json5` 覆盖到新工程对应位置；
3. 在 `entry/src/main/cpp/CMakeLists.txt` 中 include 本目录的 `CMakeLists.txt`，
   并设置 `GODOT_SOURCE_DIR` 指向 Godot 源码根目录；
4. 将 SCons 交叉编译产出的引擎静态库（`bin/libgodot.ohos.editor.arm64.a`）加入链接；
5. 选择设备模拟器 `MateBook Pro (arm64)`，构建并运行 `.hap`。

## 状态

- 第 10 轮（收尾，提交 `ac1f9ca` / `2249012`）：全链路构建已打通（`build_hap.sh` 一键出 `entry-default-signed.hap`）；
- 第 10 轮修复：模拟器「创建项目后引擎卡住」根因修复（Vulkan 上下文初始化 + tsfn 跨线程调用 + **项目打开改进程内重启**）；
- 剩余：模拟器全流程实测（见下方「模拟器验证」）。

## 模拟器验证

```bash
./build_hap.sh        # 1. 交叉编译产物已内置，直接构建签名 HAP
./run_verify.sh       # 2. 安装+启动+截屏+hilog+诊断文件（需模拟器在线）
```

> **重要**：模拟器必须从 DevEco Studio 的 **Device Manager** 图形界面启动。
> 命令行 `Emulator -start` 会因缺少一次性 SN 文件失败——该文件由 IDE 点击
> 启动时写入系统临时目录（macOS 清理临时目录后必须重新用 GUI 启动）。
>
> 验证点：项目管理器显示 → 新建项目 → 「创建并编辑」→ 编辑器打开渲染
> （进程内重启生效，不再卡住）。卡住时查看 `<cacheDir>/godot_engine_diag.log`。

## 线程模型

- **ArkUI 主线程**：执行 NAPI 调用（初始化/启停）与 ArkUI 事件；
- **引擎线程**（`engine_start` 创建）：运行 `Main::setup/start/iteration` 主循环；
- **渲染线程**：Vulkan 内部管理。

详见 `docs/移植方案` 中「1.1 线程模型」。
