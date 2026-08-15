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

- **软件 Vulkan 转译层（express_gpu）**：forward_plus 特性支持不全
  （D16 采样纹理等大量 ERR 属预期），编辑器渲染时模拟器自身可能崩溃
  （EMULATOR_CRASH 802002）——**环境限制，非移植代码问题**；
- 若转译层不稳定：Index.ets 取消注释 `godot.setRenderingMethod('mobile')`
  （mobile 渲染器特性需求少）；
- 渲染画面最终验证建议真机（MateBook Pro 2in1）。

## 5. 常见问题

- **HAP 构建偶发失败（hvigor 00303168）**：与 DevEco Studio 的 hvigor
  守护进程竞争，删除 `.hvigor` 与 `entry/build` 后重试即可；
- **scons 增量检测**：修改源文件后务必确认输出含
  `Compiling shared platform/ohos/<file>`；未出现时删除对应 .o 后重跑；
- **模拟器崩溃后 CLI 起不来**：qemu 不 spawn（快照损坏/对话框阻塞），
  必须回 DevEco GUI 启动。
