# M07 音频 Spec（基于代码核对 2026-08-17）

## 代码事实
- 输出：audio_driver_ohos.cpp 真实实现（OHAudio 渲染流 builder/mix/start 链路，18 处 API 引用）
- 输入（RecordingStream）：❌ 完全未实现

## 任务
- [ ] T-AU-1 输出验证：模拟器启动查音频驱动初始化日志（不崩即过）
- [ ] T-AU-2 输入实现：RecordingStream + 引擎输入接口

## 开放项
- 出声验证无环境（模拟器无真实音频设备）。