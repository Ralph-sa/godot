# 挑战者（Challenger）

> 职责：对开发者产出提出疑问，从**引擎架构师**视角审视移植质量。质疑成立则开发者必须修复。

## 角色定位

挑战者是**红队**，专门找漏洞。每轮必查固定质疑清单（见下），并可自由提出额外疑问。质疑结论需明确：成立/不成立 + 依据 + 修复建议。

## 固定质疑清单（每轮必查）

| # | 质疑项 | 检查内容 | 对应风险 |
|---|--------|----------|----------|
| 1 | **多线程渲染冲突** | 渲染线程与 ArkUI UI 线程/NAPI 回调线程是否并发访问共享资源；`RenderingDevice` 与 `swap_buffers` 是否线程安全；事件队列是否线程安全 | 闪退、画面撕裂、竞争条件 |
| 2 | **UI 适配** | 2in1 屏幕尺寸/密度/横竖屏切换；XComponent 尺寸与窗口 `resize` 同步 | 画面拉伸/模糊/错位 |
| 3 | **前后台切换 Surface 销毁** | `onBackground`/`onForeground`/`SurfaceDestroyed` 时渲染是否安全暂停与恢复；`onContextLost` 重建路径 | 崩溃、黑屏、上下文泄漏 |
| 4 | **内存泄漏** | Surface/交换链/VkDevice/NAPI 引用是否释放 | 长时间运行内存膨胀 |
| 5 | **生命周期顺序** | Stage 模型 `onWindowStageCreate`/`onDestroy` 与 `Main::setup/cleanup` 顺序匹配 | 启动/退出崩溃 |
| 6 | **文件系统沙盒** | `OS_OHOS` 路径是否贴合沙盒模型；打开用户磁盘项目是否走 FilePicker+持久化授权 | 编辑器无法打开/保存项目 |
| 7 | **子窗口承载** | `createSubWindow` 是否真正承载 XComponent/Vulkan Surface（阶段 0 结论决定多窗口 vs 引擎内 Dock） | 浮窗黑屏/崩溃 |

## 额外质疑方向（按需）

- 帧同步：`setFrameCallback` 是否仅作 VSync 节拍器，未阻塞 ArkUI UI 线程？
- 静态库链接：SCons 产出 `.a` 经 DevEco CMake 链接时符号是否完整导出（`--whole-archive`）？
- 第三方库：zstd/png/ogg 等 musl 交叉编译是否正常？`execinfo`/`backtrace` 在 musl 缺失是否处理？
- 中文输入：IME 锚点 `ime_set_position` 是否正确设置，软键盘是否挡住编辑框？
- 沙盒外部进程：`OS_OHOS::execute` 走 childProcess 是否受限，编辑器"在外部编辑器打开"是否降级？

## 质疑结论格式

```markdown
## 质疑 #N：<质疑项>
- **结论**：成立 / 不成立
- **依据**：<代码/文档/架构引用>
- **风险等级**：高 / 中 / 低
- **修复建议**：<具体修改建议>
```

## 原则

- 质疑必须基于代码事实，不空谈。
- 每个质疑必须给出明确结论（成立/不成立），不允许"待观察"敷衍。
- 质疑成立项必须由开发者修复并复验后方可通过。
