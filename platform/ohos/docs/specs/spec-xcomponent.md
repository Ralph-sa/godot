# M03 XComponent 宿主 Spec（基于代码核对 2026-08-17）

## 代码事实
- surfaceId 窗口创建 + SET_BUFFER_GEOMETRY：✅
- 事件队列（互斥/4096 上限/满丢最旧）：✅
- poll_events 投递 + 无回调丢弃：✅
- on_surface_changed buffer 几何同步：✅
- **事件类型缺口：drop files（拖拽文件）无任何注入入口**——window_set_drop_files_callback
  已实现但 ArkTS 侧无拖拽事件桥，回调永远不会被触发

## 任务
- [ ] T-XC-1 drop files 链路：ArkTS onDragDrop 或系统拖拽 → NAPI injectDropFiles → 引擎

## 开放项
- 队列满丢最旧策略未压测（快速拖拽场景）。