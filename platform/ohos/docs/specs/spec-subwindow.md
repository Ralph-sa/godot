# M10 子窗口与多窗口 Spec（基于代码核对 2026-08-17）

## 代码事实
- 生命周期：✅ 完整——create_sub_window/show/delete（引擎侧）+ ohos_subwindow_* NAPI 桥 + ArkTS handleSubWindow（TYPE_FLOAT 窗口真实创建）
- **输入分发到子窗口：❌ 缺失**（全部注入主窗口）
- **window_move_to_foreground：❌ 空壳**（M02 T-DS-3）——子窗口弹出后无法置前
- **window_set_transient：❌ 空壳**（M02 T-DS-4）

## 任务
- [ ] T-SW-1 子窗口输入分发（坐标/焦点判断窗口归属）
- [ ] T-SW-2 模拟器验证子窗口创建+置前链路（依赖 T-DS-3）

## 开放项
- 模拟器画面不刷新——子窗口验证只能到窗口管理 API 层面。