# Godot 鸿蒙编辑器 · 模拟器实测指南（2026-08-16）

## 0. 前置条件

- 模拟器必须从 **DevEco Studio 的 Device Manager 图形界面启动**。
  命令行 `Emulator -start` 不可用：DevEco 启动时会通过内部通道向模拟器
  注入一次性 SN 凭证（macOS 清理临时目录后旧凭证失效），CLI 启动会卡在
  「can not read uuid file」错误。
  > 本机调试备选：`Emulator.app/Contents/emulator/Emulator.patched`
  >（本地制作的调试版，绕过 SN 文件检查，签名已重建；正式使用仍走 GUI）。

## 1. 构建与部署（模拟器在线后）

```bash
cd godot/platform/ohos/deveco
./build_hap.sh          # 签名 HAP：entry/build/default/outputs/default/entry-default-signed.hap
./run_verify.sh         # 安装 + 启动 + 截屏 + hilog + 拉取诊断文件
```

## 2. 验证清单

1. 截屏 1（15s）应显示 **Godot 项目管理器**（证明 Vulkan 渲染链路 OK）；
2. 项目管理器「新建项目」→ 创建 →「创建并编辑」；
3. 编辑器打开（不再卡住/黑屏）——**进程内重启**（OS_OHOS::create_instance）生效；
4. 鼠标/键盘/触摸可操作编辑器——**ArkTS 事件桥**（injectMouse/Key/Touch）生效；
5. 文件对话框「打开项目」可读公共目录——**URI 沙盒拷贝**生效。

## 3. 诊断文件（应用沙盒 cacheDir）

`/data/app/el2/100/base/com.godot.editor/haps/entry/cache/` 下：

| 文件 | 内容 |
|---|---|
| godot_engine.log | 引擎全部日志（[I]/[E] 前缀，print handler 写入） |
| godot_engine_diag.log | 引擎线程生命周期（setup/start/iteration/cleanup/restart 阶段） |
| godot_ds_diag.log | DisplayServer Vulkan 初始化各步骤返回码 |
| godot_xc_diag.log | surfaceId 窗口创建结果 |
| godot_crash.log | 崩溃信号信息（信号处理器写入） |

`hdc file recv <path> <local>` 拉取。hilog 在 DeviceDebuggable:No 设备上
全部脱敏为 `<private>`，以上文件是唯一可靠诊断通道。

## 4. 已知模拟器限制

- **软件 Vulkan 转译层（express_gpu）崩溃根因已定位（2026-08-16 两轮 lldb 实测）**：
  宿主进程在 vk_decode_invoke + 29588 处 free 了一个非 malloc 分配的指针
  （___BUG_IN_CLIENT_OF_LIBMALLOC_POINTER_BEING_FREED_WAS_NOT_ALLOCATED 即 abort），
  触发序列固定为 vkCmdPipelineBarrier + vkCmdCopyBuffer(1 region) + vkQueueSubmit
  （Godot 每帧 uniform 上传路径 100% 触发；系统 UI 偶发触发）。
  **纯模拟器 bug（6.1.1.350 / 镜像 6.1.0.125，无可用更新），与移植代码无关**；
- 已尝试的规避：低分辨率实例（1560x1040，系统 UI 稳定 2 分钟）+
  mobile 渲染器（崩溃点不变，仍为 vk_decode_invoke+29588）——**无效**；
- forward_plus 特性支持不全（D16 采样纹理等大量 ERR 属预期）；
- **渲染画面验证必须走真机**（MateBook Pro 2in1）：真机 GPU 原生 Vulkan，
  无转译层，本 bug 不存在；模拟器只能验证到「引擎初始化全绿 + Vulkan 命令
  提交」这一阶段。
emu 不 spawn（快照损坏/对话框阻塞），
  必须回 DevEco GUI 启动。
