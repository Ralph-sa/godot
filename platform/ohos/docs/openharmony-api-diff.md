# OpenHarmony 6.0 与 HarmonyOS 6.1.1 (API 24) API 差异分析

> 生成日期：2026-08-16
> 目的：为 Godot 鸿蒙移植（`platform/ohos`）提供源码参考与 API 差异地图，
> 回答「改用 OpenHarmony 源码/SDK 是否更有效、能否装到鸿蒙手机」两个关键问题。
> 本机源码参考目录：`openharmony-src/`（见其 README）。

## 1. 版本背景与结论速览

| 系统 | 版本 | API version | 源码可得性 | 说明 |
|---|---|---|---|---|
| OpenHarmony | 6.0 Release | 20 | 开源（本机已拉，分支 OpenHarmony-6.0-Release） | 社区稳定基线 |
| OpenHarmony | master（7.0 开发线） | 未发布 | 开源 | 社区 7.0 尚未发 release 分支 |
| HarmonyOS | 6.1.1 | 24 | 闭源（DevEco SDK 头文件可见） | 本工程编译目标 |
| HarmonyOS（真机 SGT-AL10） | 7.0.0.100 Beta2 | 26 | 闭源 | 基于 OpenHarmony 7.0.0 开发线 |

**核心结论（详见各节）：**

1. Godot 平台层所用的全部系统 native API（XComponent、OHNativeWindow、OHAudio、IME C API、
   rawfile、hilog、napi、EGL_KHR_platform_ohos、VK_OHOS_surface）在 **OpenHarmony 6.0 (API 20)
   与 HarmonyOS 6.1.1 (API 24) 之间绝大多数一致或向后兼容**，差异集中在少量新增 API 与枚举补充。
   具体逐项见 §4 各分域比对表。
2. **移植链路的真实瓶颈不在 API 差异**，而在：DevEco 模拟器稳定性（本机 Emulator 6.1.1.350 崩溃）、
   模拟器 SN 凭证机制、以及 native 行为文档缺失。OpenHarmony 源码的价值是**查实现行为**（§3）
   与**换运行环境**（真机/OpenHarmony 设备），不是换编译 SDK。
3. **可安装性**：真机 API 26 ≥ 工程 API 24 ≥ OpenHarmony 6.0 API 20。
   用 OpenHarmony 6.0 SDK 编译的 HAP（仅用开源基础 API）可安装到 HarmonyOS 真机，
   前提是**华为 AGC 签名**（调试证书即可，与用哪个 SDK 编译无关）。
   反向：本工程若用了 API 24 的 NEXT 专有接口，则纯 OpenHarmony 设备装不上（Godot 目前未用，见 §4）。
4. **API 26 行为变化（真机已遇）**：HarmonyOS 7.0 移除了 XComponent onLoad 上下文的
   nativeXComponent 属性——§4.10 溯源证明该属性本是**华为商业运行时的私有扩展**，
   OpenHarmony 开源主线从未提供（开源机制为 `__NATIVE_XCOMPONENT_OBJ__` 注入 + exports 对象
   即 context）；移除后华为侧反而与开源行为对齐。surfaceId 路径才是两侧共有的长期契约，
   Godot 第 10 轮适配方向正确。

## 2. 比对方法与两侧路径

- A 侧（开源）：`openharmony-src/` 下 12 个仓库，分支 OpenHarmony-6.0-Release（third_party_egl 为 manifest 指定的 master）。
- B 侧（闭源 SDK 头文件）：`/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/`
  - native：`native/sysroot/usr/include/`
  - ArkTS：`ets/api/` 与 `ets/kits/`
- 比对对象：Godot 平台层与 ArkTS 桥接层**实际使用的 API**（符号清单自动提取自 `platform/ohos/*.cpp|h`
  与 `deveco/entry/src/main/ets/*.ets`，工作区 `.dsh-tools/api-analysis/`）。
- 状态标记：✓ 一致 / △ 签名或值有差异 / ✗ 一侧缺失。

## 3. OpenHarmony 源码对移植的排错价值（怎么用）

Godot 平台层是 native C++，直接调用系统库。DevEco SDK 只给头文件，
而 OpenHarmony 源码能回答头文件回答不了的行为问题：

| 排错问题 | 查哪个仓库 |
|---|---|
| XComponent surface 生命周期（surfaceCreated/Destroyed 回调时序、surfaceId 有效期） | `openharmony-src/arkui_ace_engine/`（ace_ndk / xcomponent） |
| EGL_KHR_platform_ohos 的 display/window 绑定行为、GLES 上下文切换 | `openharmony-src/graphic_graphic_2d/`、`third_party_egl/` |
| OHNativeWindow buffer 请求/提交语义（NATIVE_WINDOW_* opt） | `openharmony-src/graphic_graphic_surface/` |
| OHAudio 回调线程与实时性约束、低时延模式行为 | `openharmony-src/multimedia_audio_framework/` |
| IME C API 的 attach/show 前置条件（ArkTS 侧需同步做的事） | `openharmony-src/inputmethod_imf/` |
| napi tsfn 跨线程调用语义 | `openharmony-src/arkui_napi/` |

## 4. 分域差异详情

> 各分域完整比对报告在工作区 `.dsh-tools/api-analysis/`（diff-*.md）。下表为汇总；
> 符号状态：✓ 一致 / △ 有差异 / ✗ 一侧缺失。

### 4.0 汇总表

| 域 | 结论 | 对 Godot 的风险 | 关键点 |
|---|---|---|---|
| XComponent native | ✓ Godot 所用全部符号 ABI 一致 | 低 | B 侧新增 API 22 SurfaceConfig 族（未使用）；回调在 UI 线程；Destroyed 后句柄失效 |
| EGL | ✓ 头文件两侧逐字一致 | 低 | 鸿蒙 libEGL 的 eglCreatePlatformWindowSurface 返回 NO_SURFACE，Godot 用 eglCreateWindowSurface 已规避 |
| Vulkan (VK_OHOS_surface) | ✓ 完全一致 | 低 | 结构体/原型/扩展名/sType 均一致 |
| OHAudio | ✓ 所用符号一致 | 低 | B 侧新增 API 23/24 manager/latency 接口（未使用） |
| 输入法 IME C API | △ **1 个符号 B 侧独有** | **高** | `OH_TextEditorProxy_SetCallbackInMainThread` @since 22，OpenHarmony 6.0 无 |
| rawfile | ✓ ABI 兼容 | 低（另见代码隐患） | 建议 Godot 改 OpenRawFile64 支持 >2GB pck |
| hilog | ✓ 完全一致 | 低 | LOG_INIT/LOG_CORE 仅存在于系统内部头 log_c.h，NDK 无 |
| native_window / napi | （见 4.7 / 4.8） | — | — |
| ArkTS 桥接面 | （见 4.9） | — | — |

### 4.1 XComponent（native 头文件）

- Godot 使用的 15 个枚举常量、12 个类型/结构体、11 个函数在 OpenHarmony 6.0 与 API 24
  两侧**签名与枚举数值完全一致**（KeyCode 全表同值、按钮位掩码同值）。
- B 侧（API 24）新增 `OH_ArkUI_XComponentSurfaceConfig_*`（@since 22）与
  `OH_ArkUI_SurfaceHolder_SetSurfaceConfig`：OpenHarmony 6.0 没有，Godot 未使用；未来用需 API≥22 条件编译。
- 两侧 `@since` 版本号体系不同（如 SurfaceHolder 族 A 侧 @since 18 / B 侧 @since 19），仅注释差异。
- 实现行为（源自 `arkui_ace_engine` 源码）：
  - surface 生命周期回调（OnSurfaceCreated/Changed/Destroyed）运行在 **ArkUI UI 线程**
    （`xcomponent_pattern.cpp` 各回调 `CHECK_RUN_ON(UI)`）；回调内不可长时间阻塞。
  - OnSurfaceDestroyed 后 nativeWindow 句柄被置空，不可再使用。
  - Touch/Mouse/Key 事件数据仅在回调期间有效，需在回调内取走。

### 4.2 EGL（EGL_KHR_platform_ohos）

- `eglplatform.h`、`egl.h`、`eglext.h` 两侧**逐字一致**（仅 CRLF/LF 差异）。
  `EGL_PLATFORM_OHOS_KHR=0x34E0`、`EGLNativeWindowType` 等全部一致。
- 行为要点（`graphic_graphic_2d/frameworks/opengl_wrapper`，OpenHarmony 的 libEGL 实现）：
  - `eglGetPlatformDisplay` 在鸿蒙要求传 `EGL_DEFAULT_DISPLAY`；
  - `eglCreatePlatformWindowSurface` 在鸿蒙返回 `NO_SURFACE`——
    Godot 的 `egl_manager_ohos_gles.cpp` 已用 `eglCreateWindowSurface` 规避，**无需改动**。

### 4.3 Vulkan（VK_OHOS_surface）

- Godot 唯一用到的 `VK_OHOS_surface` 扩展两侧完全一致：`VkSurfaceCreateInfoOHOS` 结构、
  `vkCreateSurfaceOHOS` 原型、扩展名宏、`sType=1000685000`。
- B 侧对 native_buffer/external_memory 扩展加了 deprecated 标注与旧 `OpenHarmony` 别名
  ——Godot 未使用，无影响。

### 4.4 OHAudio

- Godot 使用的全部 OHAudio 符号（AudioStreamBuilder 族、AudioRenderer 族、回调 typedef、
  `OH_AudioData_Callback_Result` 枚举等）两侧**签名与枚举值完全一致**。
- B 侧新增 API 23/24 的 manager/会话/latency 接口：OpenHarmony 6.0 无，Godot 未使用。
- 两侧 `OH_AudioRenderer_OnWriteDataCallback` 等 typedef 出处行号见分域报告。

### 4.5 输入法 IME C API —— 唯一高风险的 native 差异

- **`OH_TextEditorProxy_SetCallbackInMainThread(proxy, bool)`**：
  - B 侧（API 24）存在，`@since 22`；
  - A 侧（OpenHarmony 6.0 / API 20）头文件与实现**完全没有**（全仓 grep 0 命中）；
  - Godot `ime_ohos.cpp:85` 正在使用（要求 IME 回调回到 ArkUI 主线程）。
- 影响：若改用 OpenHarmony 6.0 SDK 编译 Godot 平台层，此调用**无法编译**。
  对策：`#if` 按 API 版本条件编译（≥22 才调用；≤20 时 IME 回调默认在 IPC 线程，
  需在回调内自行 PostTask 回主线程再入队）——或锁定 HarmonyOS SDK 编译目标不变。
- 其余 IME 符号（Attach/Show/Hide/Cursor/TextConfig/TextEditorProxy 回调族）两侧签名一致。

### 4.6 rawfile / hilog（低风险，附代码隐患建议）

- rawfile：`OH_ResourceManager_*` 六个符号 ABI 完全兼容（Godot 用的 napi_env 新签名两侧一致）。
  - **代码隐患（非 API 差异）**：`main_ohos.cpp` 用 `ReadRawFile` + `int` 长度，
    主包 >2GB 时有截断风险；建议改 `OpenRawFile64`/`ReadRawFile64`（@since 11，两侧均有）。
- hilog：`OH_LOG_Print`、LogType/LogLevel、五个宏两侧完全一致。
  `LOG_INIT/LOG_CORE/LOG_KMSG` 只在系统内部头 `log_c.h` 中，NDK 无——Godot 自定义的
  `OHOS_LOG_DOMAIN/OHOS_LOG_TAG` 用法正确。

### 4.7 native_window（OHNativeWindow）

- Godot 使用的 3 个符号两侧**签名完全一致**：
  - `OH_NativeWindow_CreateNativeWindowFromSurfaceId(uint64_t, OHNativeWindow**)`（B 侧 introduced 12.0.0）
  - `OH_NativeWindow_DestroyNativeWindow(OHNativeWindow*)`
  - `OH_NativeWindow_NativeWindowHandleOpt(OHNativeWindow*, int, ...)`（Operation 枚举值两侧一致）
- B 侧（API 24）新增 7 个 API 20 之后的函数（Get/SetColorSpace、Get/SetMetadataValue、
  LockBuffer、UnlockAndFlushBuffer、PreAllocBuffers）：Godot 未使用，无影响。
- 两侧结构体细节差异（Godot 均未直接用）：`BufferHandle` 在 B 侧多 `int32_t key` 字段
  （真实 ABI 布局差异）；`OHSurfaceSource` 枚举 A 侧多 `OH_SURFACE_SOURCE_LOWPOWERVIDEO`（值 5）。
- 行为要点：surfaceId 路径创建的 OHNativeWindow 生命周期与 XComponent Surface 绑定，
  Destroy 后不得再使用（与 §4.1 的句柄失效约束一致）。

### 4.8 napi（Node-API）

- 鸿蒙的 napi 是标准 Node-API（NAPI_VERSION 8）封装，**无鸿蒙专有符号差异**。
- A 侧 `arkui_napi/interfaces/kits/napi/native_api.h` 为薄封装（include `node_api.h`，核心 Node-API 头
  `node_api.h`/`js_native_api_types.h`/`node_api_types.h` 属 `third_party_node` 仓库，本机未拉，
  声明与 B 侧 sysroot 内置版同源）；Godot 用到的 `napi_define_properties`/`napi_wrap`/`napi_unwrap`/
  tsfn 族/`napi_get_cb_info` 均为标准符号。
- B 侧（API 24）新增的 napi 扩展 API（critical scope、strong/sendable reference、
  `napi_throw_business_error`、callsite 族等）：Godot 未使用，无影响。
- 相关提醒（源自 Godot 代码注释，main_ohos.cpp:1206）：API 26 真机上曾出现
  `napi_unwrap` 返回 invalid_arg 的包装结构变化，平台层已加多路径兜底——此为运行时行为差异，
  与 6.0/API 24 头文件无关，真机验证时关注。

### 4.9 ArkTS 桥接层

- **Godot DevEco 工程可用 OpenHarmony 6.0 (API 20) SDK 直接编译，无需任何 API 降级。**
- 桥接层 14 个模块、约 70 项实际调用的 API 在两侧**全部存在且签名逐字一致**（无 ✗ 项）：
  - 最高 @since 仅 14（`Window.setWindowTopmost`），其余集中在 7-12；
  - 唯一贴边项：`fs.mkdirSync(path, recursion)` 两参重载 crossplatform 变体 @since 20，
    恰好等于 API 20 上限——可编译，但不可再向下降级；
  - B 侧新增项（如 `maximize(acrossDisplay)` @since 22）均未被桥接层引用。
- 桥接层刻意避开高版本能力（surfaceId 路径 @since 9、onLoad @since 8、ArkTS 通用事件 @since 8），
  全部早于 API 20，XComponent 渲染路径完全兼容。
- 行为一致性：剪贴板、文件选择器（persistPermission @since 11）、窗口管理、输入设备枚举
  在两侧签名与语义一致。

### 4.10 XComponent 上下文 API 26 行为变化溯源（nativeXComponent 移除）

**现象（Godot 平台层在 API 26 真机实测，见 main_ohos.cpp / ohos_xcomponent.* 注释）：**
- XComponent 的 `onLoad` 上下文不再提供 `nativeXComponent` 属性（`typeof === undefined`）；
- Surface 延迟到组件首次可见才创建；
- 因拿不到 `OH_NativeXComponent` 句柄，原生回调（DispatchTouchEvent 等）无法注册，
  Godot 第 10 轮改为 **surfaceId 路径 + ArkTS 通用事件桥**（`getXComponentSurfaceId` @since 9
  → `OH_NativeWindow_CreateNativeWindowFromSurfaceId` @since 12）。

**声明层证据（两侧 .d.ts 对比）——契约从来是弱类型：**
- API 24 SDK `ets/component/xcomponent.d.ts`：`OnNativeLoadCallback = (event?: object) => void`
  （@since 18）；`getXComponentContext(): Object`（@since 12）——均未承诺 `nativeXComponent` 字段；
  且注释明示「context 包含的 API 由开发者在 native 层定义」。
- OpenHarmony 6.0 与最新 master 的 `interface_sdk-js/api/@internal/component/ets/xcomponent.d.ts`
  两侧 **IDENTICAL**，与 B 侧同构（onLoad @since 18 / getXComponentContext @since 12）。

**实现层证据（OpenHarmony 源码，6.0-Release 与 master(2026-08-15) 对照）：**
1. **开源侧从未存在名为 `nativeXComponent` 的 JS 属性**——`grep '"nativeXComponent"'`
   在 6.0、master、全 `openharmony-src/` 树均零命中。开源机制的属性名是
   `__NATIVE_XCOMPONENT_OBJ__`（宏 `OH_NATIVE_XCOMPONENT_OBJ`，native_interface_xcomponent.h:51），
   通过 `LoadModuleByName(args, OH_NATIVE_XCOMPONENT_OBJ, nativeXComponentPtr)` 在加载应用 .so 时
   注入指针（`jsi_declarative_engine.cpp:2571-2572` 新管线 / :2658-2659 旧管线，两版一致）。
   因此：**Godot 在 API ≤24 真机上读到的 `context.nativeXComponent` 属性是华为 HarmonyOS
   商业运行时的上下文扩展，不属于 OpenHarmony 开源行为。**
2. onLoad 上下文 = 应用 .so 模块的 exports 对象：`AddJsValToJsValMap(componentId, obj)`
   （jsi_declarative_engine.cpp:2578-2580）→ onLoad 触发时 `GetJSVal` 取出并经
   `SetXComponentContext` 缓存（js_xcomponent_onload_function.cpp:25-37）；
   `getXComponentContext()` 返回的正是该缓存对象：
   `args.SetReturnValue(renderContext_.Lock())`（6.0 在 js_xcomponent_controller.cpp、
   最新 master 在 js_xcomponent_controller_binding.h:35-38）——**两侧机制一致且至今未变**。
3. 与「context 对象不再支持」相关的一条独立路径：ArkUI 原生控制器 C API
   `GetXComponentContextImpl`（x_component_controller_accessor.cpp:58-67）自 6.0 起即返回空
   （`LOGE "... return context object need to be supported"`）——这是 native 控制器接口，
   与 JS 侧 getXComponentContext() 不同，但同样说明开源侧不承诺上下文对象。
4. **Surface「首次可见才创建」是华为商业行为**：开源实现是组件挂载到主渲染树即创建
   （`OnAttachToMainTree → HandleSurfaceCreated`，xcomponent_pattern.cpp:277-306 / :1850-1858，
   6.0 与 master 一致），源码中无可见性门控（grep `OnVisibleAreaChange/firstVisible` 零命中）。

**结论：**
1. `context.nativeXComponent` 是**华为 HarmonyOS 商业运行时的私有扩展**，OpenHarmony 开源主线
   从未提供（开源机制是 `__NATIVE_XCOMPONENT_OBJ__` 注入 + exports 对象即 context）。
   华为在 HarmonyOS 7.0（API 26）移除了该扩展——移除后真机行为反而与 OpenHarmony 开源主线
   **对齐**。依赖它的代码天然不能在纯 OpenHarmony 设备上运行。
2. **对 Godot 的启示**：surfaceId 路径（getXComponentSurfaceId @since 9 +
   OH_NativeWindow_CreateNativeWindowFromSurfaceId @since 12）是开源与商业两侧共同支持的
   稳定契约；Godot 第 10 轮从「华为私有扩展」迁移到 surfaceId + ArkTS 事件桥，
   **同时兼容 HarmonyOS 与 OpenHarmony 设备**，是正确且面向未来的选择；不要再依赖
   onLoad 上下文的 nativeXComponent 属性（API 26 起华为侧也已移除）。
3. 附带影响：surfaceId 路径下 `LoadModuleByName` 注入机制不一定触发（取决于组件是否声明
   libraryname），因此平台层应以 surfaceId 为唯一入口，原生回调注册只作为可选增强。

## 5. 真机验证路径（SGT-AL10，HarmonyOS 7.0.0 Beta2 / API 26）

已确认真机通过 hdc 连接（设备 `5MT0225B20000890`）。版本链：
**OpenHarmony 6.0 (API 20) ≤ 工程目标 (API 24) ≤ 真机 (API 26)**——两侧编译产物均可安装。

```bash
HDC=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc
# 当前工程（API 24）签名 HAP：
$HDC install godot/platform/ohos/deveco/entry/build/default/outputs/default/entry-default-signed.hap
$HDC shell aa start -a EntryAbility -b com.godot.editor
# 诊断：引擎日志在应用沙盒 cacheDir（见 deveco/TESTING.md 诊断文件表）
$HDC file recv /data/app/el2/100/base/com.godot.editor/haps/entry/cache/godot_engine.log .
```

要点：
- 真机 API 26 上已确认的行为变化（XComponent 上下文无 nativeXComponent、Surface 延迟创建、
  输入走 ArkTS 事件桥）已由平台层第 10 轮适配（surfaceId 路径），行为溯源见 §4.10；
- 真机安装签名要求与模拟器相同（本工程 `~/.ohos/config` 调试证书已配置）；
- 若将来用 OpenHarmony 6.0 SDK 编译（API 20），同样可装到该真机，但需处理 §4.5 的
  IME `SetCallbackInMainThread` 条件编译。

## 6. 结论与行动建议

**基于全部 9 个 native 域 + 2 个图形域 + ArkTS 桥接层的比对：**

1. **Godot 平台层的 API 面在 OpenHarmony 6.0 (API 20) 与 HarmonyOS 6.1.1 (API 24) 之间高度兼容。**
   除 IME 的 `OH_TextEditorProxy_SetCallbackInMainThread`（API 22+，B 侧独有）外，
   所有 Godot 使用的符号两侧签名/枚举值一致；EGL 头文件甚至逐字一致。
   即：**改用 OpenHarmony 6.0 源码/SDK 编译，native 侧需要改动的点只有一处条件编译（IME）；
   ArkTS 桥接层零改动。**
2. **"鸿蒙没有源码导致移植不准"的实质**：编译所需头文件并不缺（SDK 自带），缺的是行为文档。
   OpenHarmony 源码的价值 = 查实现行为（UI 线程约束、句柄生命周期、EGL 平台行为），
   本报告 §4 各域已把关键行为摘录归档，可直接当作移植行为手册用。
3. **OpenHarmony 6.0 (API 20) 编译产物可装到 HarmonyOS 真机**（真机 API 26 ≥ 20，需 AGC 签名），
   但注意华为在 API 21-24 的闭源增量（IME 回调线程控制等）在 OpenHarmony 侧没有——
   若要"两边通吃"，用 API 20 基线 + 条件编译。
4. **当前最有效的下一步不是换 SDK，而是真机验证**：现有 API 24 签名 HAP 直接装真机（§5），
   把 §4 的 UI 线程/句柄生命周期约束在真实设备上验证一遍。
5. **面向未来**：§4.10 溯源确认 API 26 移除的 onLoad 上下文 nativeXComponent 本是华为商业
   私有扩展（开源侧从未提供），surfaceId 路径是两侧共有的长期稳定契约——平台层第 10 轮
   适配方向正确，且同时兼容 HarmonyOS 与 OpenHarmony 设备，保持即可。
