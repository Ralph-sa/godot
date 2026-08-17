# M04 输入链 Spec

## 目标
触摸/鼠标/键盘从 ArkTS 到引擎 GUI 的完整链路。

## 验收标准
- ① Stack 透明层接收触摸（XComponent native 层消费触摸，直接 onTouch 无效）
- ② 坐标换算用 XComponent px/vp 实际比例（density API 不可信）
- ③ 注入鼠标语义事件（编辑器桌面 GUI 不认 ScreenTouch）
- ④ 键盘（onKeyEvent→injectKey）可用
- ⑤ 验证：T04 输入注入 PASS + 引擎 GUI 处理点击（引擎日志 ERR/print 响应）

## 状态
✅ 完成（触摸/鼠标已通）。键盘未验证。

## 剩余任务
- [ ] T-IN-1 键盘验证：模拟器注入按键，确认 injectKey 到引擎、编辑器快捷键可用。

## 开放项
- 多点触摸未支持（当前单指）——编辑器场景是否需要多点？