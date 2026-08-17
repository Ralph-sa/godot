# M06 Vulkan 渲染 Spec（基于代码核对 2026-08-17）

## 代码事实
- RenderingContextDriverVulkanOHOS：surface 创建/交换链路径已实现（screen_create 修复）
- 模拟器：禁用（转译层 vk_decode_invoke+29588 崩溃）
- Maleoon 真机 compute 管线失败：冻结（无验证环境）

## 任务
- [ ] T-VK-1 源码分析：mobile 渲染器 compute 需求 vs forward_plus（评估规避可行性）
- [ ] T-VK-2 rendering_context_driver_vulkan_ohos.h 头注释更新（已非"第 1 轮骨架"）

## 开放项
- 冻结在"代码就绪、行为未验"。