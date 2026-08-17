# M11 手柄输入 Spec

## 目标
手柄按键（已实现枚举）与摇杆轴（未实现）映射到引擎 Input。

## 现状
- 按键枚举桥已有（gamepadDevices 回传）。
- 摇杆轴：@ohos.multimodalInput.inputDevice 无轴数据 API。

## 验收标准
- ① 手柄连接/断开事件回传引擎
- ② 摇杆轴数据可用时映射到 Godot joy_axis

## 剩余任务
- [ ] T-GP-1 调研虚拟 HID 或其他轴数据来源（对照 openharmony-src）

## 开放项
- 系统 API 缺失——此项可能冻结直至鸿蒙提供轴数据能力。