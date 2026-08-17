# M06 Vulkan 渲染 Spec

## 目标
forward_plus/mobile 渲染器经 VK_OHOS_surface 在鸿蒙上运行。

## 现状
- 引擎侧：RenderingContextDriverVulkanOHOS + screen_create 交换链已就绪（模拟器曾验证初始化全绿）。
- 模拟器：Vulkan 翻译层（vk_trans）崩溃 bug（vk_decode_invoke+29588），渲染方法在模拟器禁用。
- 真机（已不再参与验证）：Maleoon 935 compute 管线创建失败（VkResult -1）→ 空管线崩溃。

## 验收标准
- ① 模拟器：Vulkan 路径标记为不可用（不回归验证）
- ② 引擎侧：display_server 构造的 vulkan 分支编译通过、无回归
- ③ 构建门禁：opengl3=yes 与 vulkan=yes 并存编译通过

## 剩余任务
- [ ] T-VK-1 确认 mobile 渲染器的 compute 需求是否少于 forward_plus（评估规避 Maleoon compute 失败的可行性——需查源码，不依赖真机测试）。

## 开放项
- Maleoon compute 失败根因未定（驱动兼容性 vs 引擎用法）——无真机验证环境，此模块冻结在"代码就绪、行为未验"。