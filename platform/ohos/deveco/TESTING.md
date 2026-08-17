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

## 2. 验证清单（2026-08-17 修订：GLES 直进编辑器 + 自动化测试）

### 2.1 自动化测试（首选）

```bash
cd godot/platform/ohos/deveco/test
python3 run_tests.py --device 127.0.0.1:5555   # 模拟器
python3 run_tests.py --device <手机序列号>     # 真机可选（T07 自动执行画面 diff）
```

T01 启动链（setup→editor→EditorNode）/ T02 渲染循环 / T03 窗口内容 / T04 输入注入 /
T05 EGL 链路 / T06 进程存活 / T07 交互响应（模拟器恒 SKIP：express_gpu
不更新帧内容，属已知限制非失败；真机在线时自动执行画面 diff）。
门禁标准 = 7 PASS（T07 模拟器 SKIP 计 PASS）。

### 2.2 手工清单（真机/模拟器逐项实测）

1. ✅（模拟器/真机）引擎直进编辑器（--editor --path 默认项目，不再经过项目管理器；
   渲染方法 gl_compatibility/GLES——旧版 Vulkan 项目管理器清单项已废弃）；
2. ✅（模拟器）编辑器完整 UI 上屏（Godot 标题/菜单栏/3D 视口/文件系统/检查器面板）；
3. ✅（模拟器）渲染循环持续出帧（swap 计数增长；真机已验证 3600+ 帧）；
4. ✅（模拟器）输入事件链（透明层 onTouch → NAPI → push_touch(鼠标语义) → poll 消费 →
   引擎 GUI 响应）；画面级交互验证（点击后界面变化）模拟器受限于不刷新帧，不列入验证范围；
5. ⏳ 文件对话框「打开项目」URI 沙盒拷贝——待验证（第 11 项遗留清单）。

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
