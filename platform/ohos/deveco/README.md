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

- 第 1 轮（骨架期）：NAPI 入口（initialize/start/stop/dispose）+ XComponent 宿主已就绪；
- 完善期待办：
  - XComponent 输入事件分发（触摸/键鼠）接入 `DisplayServerOHOS`；
  - Surface 尺寸变化同步（`on_surface_changed` → swapchain 重建）；
  - 子窗口/浮层（编辑器工具面板）承载方案验证；
  - 引擎静态库与完整编译链路打通。

## 线程模型

- **ArkUI 主线程**：执行 NAPI 调用（初始化/启停）与 ArkUI 事件；
- **引擎线程**（`engine_start` 创建）：运行 `Main::setup/start/iteration` 主循环；
- **渲染线程**：Vulkan 内部管理。

详见 `docs/移植方案` 中「1.1 线程模型」。
