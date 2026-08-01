---
name: godot-ohos-port
description: 将 Godot macOS 平台（platform/macos）移植到鸿蒙平台（platform/ohos）的 10 轮全量迭代技能。每一轮都从 A→J 全清单贯穿执行（构建系统→OS 层→窗口宿主→DisplayServer→渲染驱动→输入/系统集成→导出器→收尾验证），实现深度逐轮递增（骨架期→功能期→完善期），每轮结束对比 macOS 源码（行数+接口覆盖率）与测试结果输出完成度报告，功能写完验证通过即 git 提交，然后重启下一轮，反复打磨共 10 轮直至完全移植，第 10 轮结束产出最终总结 MD，坚持真实可编译代码。使用场景：Godot 鸿蒙移植、platform 移植、macOS 代码移植到 OHOS。
---

# godot-ohos-port：Godot 鸿蒙平台移植技能

将 `platform/macos`（实际 **76 文件 / 21474 行**）完全移植到 `platform/ohos`。采用 loopengine 多 agent 概念，5 个专职角色（管理者/开发者/挑战者/审查者/测试者）协作，构成 **10 轮全量迭代循环**。本技能由以"资深游戏开发者 / 引擎架构师 / 产品设计师"三重身份驱动的执行 agent 落地。

## 1. 轮次语义（核心）

共 10 轮，**每一轮都从 A→J 全清单贯穿执行**，实现深度逐轮递增，第 10 轮达到完全移植：

- **第 1~2 轮 · 骨架期**：全链路先打通**最小可编译闭环**（构建系统 → OS 层 → 窗口宿主 → DisplayServer → 渲染驱动 → 导出器骨架 → 验证）。目标：产出能交叉编译、DevEco 工程能链接、模拟器能启动打印版本号的完整产物。
- **第 3~6 轮 · 功能期**：DisplayServer 窗口 API、Vulkan 渲染驱动、输入栈（键盘/鼠标/笔/触摸/IME/剪贴板/光标/拖放）逐层补全真实实现，编辑器画面在模拟器正常渲染。
- **第 7~10 轮 · 完善期**：桌面集成（多窗口/音频/系统集成）、导出器完整、模拟器实测打磨、对比 macOS 补齐遗漏接口，`porting-matrix.md` 达 100%。

每轮结束必须对比总结 macOS 代码与测试结果，然后**重启轮次**进入下一轮，在前一轮基础上继续完善，直到第 10 轮达到完全移植。

## 2. 五角色协作循环（loopengine 驱动规则）

每一轮由管理者编排，按 `管理者 → 开发者 → 挑战者 → 开发者(修复) → 审查者 → 测试者` 顺序推进，形成闭环：

```text
第 N 轮开始（N = 1..10）
  ┌─ 管理者: 从全量清单按依赖顺序（A→J）整理该轮 tasklist，设定验收标准（XComponent 桥接/渲染正常/无内存泄漏）
  ├─ 开发者: 按 tasklist 逐项编写 ArkTS + C++ 桥接层代码（真实可编译、中文注释）
  ├─ 挑战者: 对产出提出疑问（多线程渲染冲突 / UI 适配 / 前后台切换 Surface 销毁风险 / 内存泄漏）
  ├─ 开发者: 修复挑战者提出的疑问（质疑成立即修改代码）
  ├─ 审查者: 检查 @ohos 接口规范 / 鸿蒙生命周期写法 / 资源释放逻辑 / 中文注释
  ├─ 测试者: DevEco CLI 编译 → 启动模拟器 → 检测闪退 / 画面正常渲染
  │    ├─ 失败 → 管理者把修复项加入本轮 tasklist → 回到开发者
  ├─ 对比总结: 对比 macOS 源码（diff_summary.py 行数+接口覆盖率）+ 测试结果 → 输出完成度报告
  ├─ git 提交: 功能写完 + 验证通过后，在 hm 分支 git commit（信息含轮次），作为本轮完成标志
  └─ 重启: 若 N < 10 进入第 N+1 轮（在既有基础上加深实现深度）；N == 10 输出最终总结 MD
```

**每轮内依赖顺序（强制）**：构建系统(A) → OS 层(B) → 窗口宿主(C) → DisplayServer(D) → 渲染驱动(F) → 嵌入式(E) → 输入/菜单(G) → 系统集成(H) → 导出器(I) → 收尾验证(J)。**构建必须先通**；渲染依赖 DisplayServer 与窗口宿主；不可跳级开发导致无窗口代码先行。

### 技能每次被调用时

1. 读 `round-progress.md` 确定当前轮次 `N` 与角色结论。
2. **管理者**（`agents/manager.md`）：从全量移植清单**整理该轮 tasklist**（任务项 + 优先级 + 验收标准）。
3. **开发者**（`agents/developer.md`）：**按 tasklist 逐项开发**桥接层代码。
4. **挑战者**（`agents/challenger.md`）：从固定质疑清单（见第 4 节）审视产出，提出疑问；**开发者修复**质疑成立项。
5. **审查者**（`agents/reviewer.md`）：按 `@ohos` 接口规范、鸿蒙生命周期、资源释放、中文注释四项检查；`scripts/check_build.py` 编译校验。
6. **测试者**（`agents/tester.md`）：`scripts/run_emulator.sh` 调 DevEco CLI 编译 → 启动模拟器 → 检测闪退与渲染。
7. **对比总结**：`scripts/diff_summary.py` 对比 macOS 源码输出该轮完成度 + 汇总测试结果；管理者更新 `round-progress.md` 记录各角色结论。
8. **git 提交**：功能写完且验证（编译/审查/测试/冒烟）全部通过后，在 hm 分支执行 `git commit`（提交信息含轮次与完成内容），作为本轮完成标志；编译损坏时回滚 `round_N_start` 后重做。
9. **重启轮次**：若 `N < 10` 输出"第 N+1 轮重启，重点改进项：xxx"；`N == 10` 输出**最终总结 MD**（见第 7 节）。

> 质量门槛：编译校验（`check_build.py`）、挑战者质疑、审查者规范检查、测试者模拟器运行，四者任一不过则该轮不通过，回到开发者修复。

## 3. 五角色职责定义（对应 agents/ 目录）

| 角色 | 文件 | 职责 | 产出/标准 |
|---|---|---|---|
| **管理者** | `agents/manager.md` | **整理该轮要做的 tasklist**（从全量移植清单提炼可执行任务项，标注优先级/依赖/验收标准）；调度其他角色；汇总对比总结（macOS 对比 + 测试结果）与验收结论 | **该轮 tasklist** + 对比总结 + 验收结论，写入 `round-progress.md`，输出下一轮重启重点 |
| **开发者** | `agents/developer.md` | **按管理者给的 tasklist 逐项开发**（ArkTS + C++ 桥接层代码）；**修复挑战者提出的疑问**（质疑成立即修改代码）；**全部代码带中文注释** | 真实可编译代码，无占位/伪实现，注释为简体中文，tasklist 逐项落实 |
| **挑战者** | `agents/challenger.md` | 提出质疑：**多线程渲染冲突**（渲染线程 vs UI 线程）、**UI 适配**（2in1 尺寸/密度）、**应用前后台切换 Surface 销毁风险**（延迟创建/onContextLost）、内存泄漏、文件系统沙盒、子窗口承载 | 质疑清单 + 是否成立结论 |
| **审查者** | `agents/reviewer.md` | 检查 **`@ohos` 接口规范**（NDK API 用法）、**鸿蒙生命周期写法**（Stage 模型/UIAbility）、**资源释放逻辑**（Surface/交换链/上下文）、**中文注释规范** | 规范检查报告，违规必须修复 |
| **测试者** | `agents/tester.md` | 调 **DevEco CLI** 编译（`hvigorw assembleHap`）、启动 **鸿蒙模拟器**（MateBook Pro），检测是否**闪退**、**画面正常渲染** | 测试报告：构建/安装/启动/渲染/崩溃结果 + 冒烟用例通过数 |

## 4. 挑战者固定质疑清单（每轮必查）

| 质疑项 | 检查内容 | 对应风险 |
|---|---|---|
| **多线程渲染冲突** | 渲染线程与 ArkUI UI 线程/NAPI 回调线程是否并发访问共享资源；`RenderingDevice` 与 `swap_buffers` 是否线程安全；事件队列是否线程安全 | 闪退、画面撕裂、竞争条件 |
| **UI 适配** | 2in1 屏幕尺寸/密度/横竖屏切换；XComponent 尺寸与窗口 `resize` 同步 | 画面拉伸/模糊/错位 |
| **前后台切换 Surface 销毁** | `onBackground`/`onForeground`/`SurfaceDestroyed` 时渲染是否安全暂停与恢复；`onContextLost` 重建路径 | 崩溃、黑屏、上下文泄漏 |
| **内存泄漏** | Surface/交换链/VkDevice/NAPI 引用是否释放；每轮 `diff_summary` 记录泄漏风险点 | 长时间运行内存膨胀 |
| **生命周期顺序** | Stage 模型 `onWindowStageCreate`/`onDestroy` 与 `Main::setup/cleanup` 顺序匹配 | 启动/退出崩溃 |
| **文件系统沙盒** | `OS_OHOS` 路径是否贴合沙盒模型；打开用户磁盘项目是否走 FilePicker+持久化授权 | 编辑器无法打开/保存项目 |
| **子窗口承载** | `createSubWindow` 是否真正承载 XComponent/Vulkan Surface（阶段 0 结论决定多窗口 vs 引擎内 Dock） | 浮窗黑屏/崩溃 |

## 5. 每轮全量移植清单（每轮都贯穿 A→J，深度逐轮递增，共 10 轮重启）

> 以下清单**每一轮都必须完整过一遍**，但实现深度逐轮递增（不是每轮都做到最终深度，也不是按轮分配任务）：
>
> - **第 1~2 轮（骨架期）**：每阶段先落地**最小可编译实现**（如 OS_OHOS 先实现路径/生命周期，DisplayServer 先实现窗口创建+渲染，导出器先出模板骨架），目标 = 全链路编译通过 + 模拟器能启动打印版本号。
> - **第 3~6 轮（功能期）**：DisplayServer 45 个虚函数 + 输入栈（键盘/鼠标/笔/IME/剪贴板/光标/拖放）+ 渲染驱动全量真实实现，编辑器画面渲染。
> - **第 7~10 轮（完善期）**：补齐 macOS 对比缺口、多窗口/音频/系统集成、导出器完整、模拟器冒烟打磨，`porting-matrix.md` 达 100%。

| 阶段 | 移植内容 | macOS 源文件 → ohos 目标 | 骨架期（第 1~2 轮） | 功能/完善期（第 3~10 轮） |
|---|---|---|---|---|
| A | 构建系统 | `detect.py`、`SCsub`、`platform_config.h`、`platform_thread.h`、`msvs.py`(不需要)、`platform_macos_builders.py` → `platform_ohos_builders.py` | 编译通过 | 完整工具链/宏配置 |
| B | OS 层 | `os_macos.h/.mm`、`dir_access_macos.h/.mm` → `os_ohos.h/.cpp`、`dir_access_ohos.h/.cpp` | 路径+生命周期 | 沙盒/进程/字体全量 |
| C | 应用/窗口原生宿主 | `godot_application`、`godot_application_delegate`、`godot_window`、`godot_window_delegate`、`godot_content_view`、`godot_button_view` → `ohos_xcomponent`、`ohos_window`(NAPI 桥) | 主窗口 Surface 创建 | 子窗口/多窗口 |
| D | DisplayServer 核心 | `display_server_macos_base`、`display_server_macos` → `display_server_ohos` | 窗口+渲染启用 | 45 虚函数全量 |
| E | DisplayServer 嵌入式 | `display_server_macos_embedded`、`embedded_gl_manager`、`embedded_debugger`、`editor/embedded_*` → `display_server_ohos_embedded`(可选) | 骨架 | 可选增强 |
| F | 渲染驱动 | `rendering_context_driver_vulkan_macos`、`gl_manager_macos_legacy`、`gl_manager_macos_angle`、`platform_gl.h`、`platform_egl.h` → `rendering_context_driver_vulkan_ohos` | surface+swapchain | 上下文重建/验证层 |
| G | 输入/光标/菜单 | `key_mapping_macos`、`godot_core_cursor`、`native_menu_macos`、`godot_menu_*`、`godot_open_save_delegate`、`godot_status_item`、`godot_progress_view` → `key_mapping_ohos`、`clipboard_ohos`、`ime_ohos` | 键鼠基本事件 | IME/剪贴板/光标/拖放 |
| H | 系统集成 | `tts_macos`、`crash_handler_macos`、`stack_trace_macos`、`libgodot_macos`、`godot_main_macos` → `tts_ohos`(可选)、`crash_handler_ohos`、`main_ohos` | NAPI 入口+生命周期 | 崩溃处理/音频/系统集成 |
| I | 导出器 | `export/export.cpp`、`export/export_plugin`、`export/logo.svg`、`doc_classes` → `export/` + `deveco/` 工程模板 | 模板骨架 | 打包签名完整 |
| J | 收尾验证 | 全量 `check_build.py`、模拟器冒烟、`diff_summary.py` 完成度报告、填充 `porting-matrix.md` | 编译+版本号 | 冒烟全通过+100% |

**每轮结束时必须输出（对比总结 + 重启依据）**：

- `scripts/diff_summary.py` 对比 macOS 源码：已移植文件数 / 未移植文件数 / **行数完成度百分比 + 接口覆盖率**；
- 测试者汇总：构建是否通过、模拟器是否启动、渲染是否正常、崩溃与否、冒烟用例通过数；
- 管理者的下一轮重点改进项清单（哪些文件还需加深、哪些测试问题要修）；
- **git 提交**：本轮功能写完且全部验证通过后，在 hm 分支 `git commit`（信息含轮次与完成内容），作为本轮完成标志；
- 更新 `porting-matrix.md` 状态列与 `round-progress.md`，然后重启第 N+1 轮。

## 6. 关键执行约束

- **每轮全清单贯穿**：每一轮都要完整过一遍第 5 节全量清单（A~J），深度逐轮递增（骨架期→功能期→完善期），不按轮拆分任务；第 1~2 轮打通最小可编译闭环，后续轮次补全实现并修复上轮遗留问题。
- **每轮内依赖顺序强制**：A(构建)→B(OS)→C(窗口宿主)→D(DisplayServer)→F(渲染)→E(嵌入式)→G(输入)→H(系统集成)→I(导出器)→J(验证)；构建必须先通，不跳级。
- **每轮结束必须对比总结**：用 `diff_summary.py` 对比 macOS 代码（**行数 + 接口覆盖率**双维度），汇总测试者结果，管理者输出下一轮重启重点改进项，然后重启第 N+1 轮，共 10 轮。
- **接口覆盖率门槛**：`DisplayServer`/`OS`/`RenderingContextDriver` 等基类的纯虚函数必须 100% override（编译器强制，编译过即满足）；公开接口覆盖度逐轮提升，第 10 轮对比 macOS 达到 100%。
- **测试者失败闭环**：测试者报告构建/渲染/冒烟任一失败 → 管理者把修复项加入本轮 tasklist → 回到开发者，通过后才进入对比总结。
- **git 提交机制（功能写完验证通过即提交）**：每轮功能写完且编译/审查/测试/冒烟全部通过后，立即在 hm 分支 `git commit`（提交信息含轮次与完成内容），作为本轮完成标志；**"功能写完、没问题"即提交，不攒批**；每轮至少一个提交，10 轮结束共 10+ 个提交，便于追溯与回滚。
- **回滚机制**：每轮开始前 `git tag round_N_start` 打点；本轮编译不可恢复地损坏时，`git checkout round_N_start` 回滚后重做（hm 分支工作树，绝不破坏主分支）。
- **五角色顺序固定**：管理者（整理 tasklist）→ 开发者（按 tasklist 开发）→ 挑战者（提出疑问）→ 开发者（修复疑问）→ 审查者 → 测试者；任一角色结论不过则本轮不通过。
- **管理者负责整理 tasklist**：每轮开始由管理者从全量移植清单提炼该轮 tasklist（任务项/优先级/依赖/验收标准），开发者只按 tasklist 开发，不自行跳项。
- **管理者验收三目标**：XComponent 桥接代码产出、模拟器正常渲染、无内存泄漏——每轮必须显式验收。
- **测试者必须实测**：测试者需真实调用 DevEco CLI 编译并启动模拟器（可用 `run_emulator.sh`），执行内置冒烟用例，不得虚构测试结果。
- **中文注释规范（硬性）**：
  - 所有新建/修改代码（C++/ArkTS/Python/Shell）必须包含简体中文注释；
  - 注释说明"做什么"与"为什么"（非显而易见意图），不写废话式注释（如"// 定义变量"）；
  - 头文件/类/关键函数必须有关键注释；平台桥接处（NAPI、生命周期、Surface 回调）必须注明鸿蒙语义；
  - 审查者将"中文注释覆盖率与质量"作为规范检查项，缺失即判违规。
- 每轮结束必须输出：该轮完成的功能点、编译校验结果、五角色结论摘要、macOS 对比完成度（行数+接口）、测试结果、git 提交哈希、下一轮重启预告。
- **第 10 轮结束输出最终总结 MD**：功能全部写完并验证通过、git 提交后，按第 7 节产出 `final-summary.md` 作为移植结束交付物。
- 真实原则硬性门槛：出现"暂不支持/伪实现/占位返回"则视为未完成该文件移植。
- 与既有方案衔接：`platform/ohos` 目录结构、detect.py/SCsub 写法、DevEco 工程模板均遵循 `.cursor/plans/godot_鸿蒙平台移植方案_c74e4e08.plan.md`（含线程模型 1.1 与沙盒文件访问 1.2 约束）。

## 7. 最终总结 MD（第 10 轮结束后产出）

第 10 轮完成且验证通过、git 提交后，输出一份**最终总结 MD**（`final-summary.md`，即本技能目录下），作为移植结束交付物，内容含：

- **移植总览**：`platform/ohos` 与 `platform/macos` 的文件数/行数对比、行数完成度、接口覆盖率；
- **与 macOS 代码对比总结**：A~J 各阶段移植情况、关键实现差异（如 NSApplication→Stage 模型、Cocoa→ArkUI/XComponent、Metal/Vulkan→Vulkan OHOS）、移植中判定"不需要/可选"的文件及理由；
- **测试结果汇总**：10 轮测试者报告摘要（构建/模拟器/渲染/崩溃/冒烟用例），编辑器功能全链路验证结论；
- **关键架构决策**：线程模型、沙盒文件访问、Surface 生命周期、多窗口策略（阶段 0 结论）等落地情况；
- **遗留问题与后续建议**：真机验证项、性能优化、可选项（TTS/嵌入式/剪贴板图片等）、社区后续维护点；
- **git 提交记录**：10 轮各轮提交哈希与说明。

## 8. 目录结构

```text
godot/.cursor/skills/godot-ohos-port/
├── SKILL.md                  # 本文件：5 角色协作 + 每轮全量移植清单 + 10 轮重启驱动规则
├── agents/
│   ├── manager.md            # 管理者：整理 tasklist、调度轮次、汇总进度
│   ├── developer.md          # 开发者：按 tasklist 开发、修复挑战者疑问
│   ├── challenger.md         # 挑战者：质疑（多线程/UI 适配/Surface 销毁/沙盒/子窗口）
│   ├── reviewer.md           # 审查者：@ohos 接口规范/鸿蒙生命周期/资源释放/中文注释
│   └── tester.md             # 测试者：DevEco CLI 编译、模拟器启动、闪退/渲染检测
├── porting-matrix.md         # macOS→ohos 文件映射矩阵（76 文件逐项对照）
├── round-progress.md         # 轮次进度追踪（记录已完成轮次/完成度%/角色结论）
├── final-summary.md          # 最终总结 MD（第 10 轮结束后产出，移植结束交付物）
└── scripts/
    ├── check_build.py        # 校验 SCons 交叉编译是否真实通过
    ├── diff_summary.py       # 对比 macOS/ohos 同名文件行数+接口覆盖率
    ├── run_emulator.sh       # 调 DevEco CLI/hdc 启动模拟器并跑冒烟测试
    └── progress.py           # 读写 round-progress.md 推进轮次
```
