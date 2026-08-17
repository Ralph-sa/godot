# GodotHMOS 移植遗留项状态表（2026-08-17 规格驱动复查）

| # | 遗留项 | 状态 | 验证手段 | 备注 |
|---|---|---|---|---|
| 1 | 真实设备/模拟器联调（渲染/输入/IME/音频/授权） | ✅ 大部分完成 | run_tests.py 7 项 | 渲染与输入已通；IME/音频待测 |
| 2 | 手柄摇杆轴 | ⏳ 待系统 API | — | @ohos.multimodalInput.inputDevice 无轴数据，需虚拟 HID |
| 3 | Vulkan surface 重建（旋转/尺寸剧变） | ⏳ 待真机 | 旋转切换测试 | GLES 路径已通；Vulkan 路径待真机 |
| 4 | 文件 URI 跨会话读取（fileAccess.open 引擎侧） | ❌ 未实现 | — | 需实现 + 验证用例（先写测试再修） |
| 5 | 子窗口输入分发 | ❌ 未实现 | — | 编辑器浮窗场景；需验证用例 |
| 6 | 文件对话框参数（初始目录/过滤器透传） | ⏳ 部分 | — | 鸿蒙 DocumentViewPicker 能力受限 |
| 7 | 音频输入链路（麦克风/中断处理） | ❌ 未实现 | — | 需 OHAudio RecordingStream |
| 8 | 鼠标捕获模式（系统级鼠标锁） | ⏳ 部分 | — | 相对位移已实现；系统锁依赖系统能力 |
| 9 | 仅 Vulkan（无 GLES3 路径） | ✅ 已过时 | — | 第 11 轮已加 GLES3/EGL 通道，此项关闭 |
| 10 | 签名/图标/隐私声明生产化 | ⏳ 调试签名可用 | — | 生产发布需 AGC 正式证书 |
| 11 | 真机 2in1 逐项验证 | ⏳ 手机已验 | 真机 | Mate 80 Pro Max 已验证编辑器渲染；2in1 待 |

## 本轮新增发现（已修复）

- window_force_make_current 缺 INVALID_WINDOW_ID 防护（点击触发子窗口时 -1 越界报错）
- 模拟器已知限制：express_gpu eglSwapBuffers 不更新帧内容（T07 模拟器 SKIP，真机执行）