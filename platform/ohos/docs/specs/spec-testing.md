# M14 测试体系 Spec（基于代码核对 2026-08-17）

## 代码事实
- run_tests.py 7 项（T01-T07）参数化：✅
- 基线：模拟器 7 PASS（T07 SKIP 计 PASS）
- T04 依赖 hilog（input_diag 文件 NAPI 主线程 fopen 失败）

## 任务
- [ ] T-TS-1 新增 T08 键盘注入测试（配合 T-IN-1）
- [ ] T-TS-2 新增 T09 音频驱动初始化检查（配合 T-AU-1）

## 开放项
- 无。