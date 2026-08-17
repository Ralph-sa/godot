# M14 测试体系 Spec

## 目标
run_tests.py 七项自动化测试作为唯一验证门禁（模拟器）。

## 验收标准
- ① 七项测试全部 PASS（T07 模拟器 SKIP 计 PASS）
- ② 测试脚本参数化（--device/--tap/--wait）
- ③ 每项修复后跑全链，FAIL 不提交

## 状态
✅ 完成（2026-08-17 基线 7 PASS）。

## 开放项
- T04 依赖 hilog（input_diag 文件在 NAPI 主线程 fopen 失败）——若后续修复 fopen 可回退文件检测。