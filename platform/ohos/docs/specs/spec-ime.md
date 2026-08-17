# M08 IME 输入法 Spec

## 目标
编辑器文本输入支持中文（inputmethod C API + ArkTS 桥）。

## 现状
- ime_ohos.cpp 已实现（attach/show/detach + 组合文本注入），未验证。

## 验收标准
- ① 文本控件聚焦时 IME attach 成功（诊断日志）
- ② 组合文本经 push_input_event 注入引擎
- ③ 验证：模拟器输入框聚焦 + 注入文本后引擎日志出现按键事件

## 剩余任务
- [ ] T-IM-1 模拟器验证 attach 链路（窗口聚焦 → IME attach 日志）

## 开放项
- 模拟器 IME 服务行为可能缺省；中文输入未验。