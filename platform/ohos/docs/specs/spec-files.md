# M09 文件对话框与 URI Spec

## 目标
编辑器文件对话框：打开/保存项目文件经鸿蒙 DocumentViewPicker + 沙盒拷贝。

## 现状
- 已完成：picker 调用、URI→沙盒拷贝、pendingExports 回拷、fileshare 持久化授权。
- 未实现：引擎侧跨会话读取 URI 文件（fileAccess.open）。

## 验收标准
- ① 文件对话框打开后返回选择结果（NAPI 桥通）
- ② 引擎侧 fileAccess.open 打开持久化授权的 URI
- ③ 验证：模拟器触发文件对话框（引擎侧调用）→ 选择器拉起（若模拟器支持）→ 结果回传

## 剩余任务
- [ ] T-FL-1 实现引擎侧跨会话 URI 读取（fileAccess.open）
- [ ] T-FL-2 验证对话框全链路（模拟器可能缺 DocumentViewPicker——先验证 NAPI 桥参数正确）

## 开放项
- DocumentViewPicker 初始目录/过滤器透传能力受限（鸿蒙 API 限制）。