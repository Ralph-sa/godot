# M03 XComponent 宿主 Spec

## 目标
OHOS_XComponent：surfaceId 窗口创建、事件队列、坐标与尺寸管理。

## 验收标准
- ① OH_NativeWindow_CreateNativeWindowFromSurfaceId 创建窗口 + SET_BUFFER_GEOMETRY
- ② 事件队列（触摸/鼠标/键盘/滚轮）带互斥与上限（4096）
- ③ on_surface_changed 同步 buffer 几何（窗口拉伸跟随）
- ④ poll_events 消费并投递（无回调时丢弃不膨胀）
- ⑤ 验证：T03 窗口内容 + T04 输入注入 PASS

## 状态
✅ 主体完成。

## 开放项
- 队列满丢弃最旧事件的策略对编辑器快速拖拽是否够用（未压测）。