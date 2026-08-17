# M07 音频 Spec

## 目标
OHAudio 输出链路出声；输入链路（麦克风）可选。

## 现状
- 输出：audio_driver_ohos 已实现（OHAudio 渲染流），未验证实际出声。
- 输入：未实现。

## 验收标准
- ① 输出：引擎启动无音频相关崩溃；OHAudio 流创建成功（诊断日志）
- ② 输入：RecordingStream 实现 + 引擎 AudioDriver 输入接口

## 剩余任务
- [ ] T-AU-1 输出验证：模拟器启动后查音频驱动初始化日志（模拟器无真实音频设备，验证到"驱动初始化不崩"）
- [ ] T-AU-2 输入实现：RecordingStream + 中断处理（参考 openharmony-src/multimedia_audio_framework）

## 开放项
- 模拟器音频设备行为未知；出声验证无环境。