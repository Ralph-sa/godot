# M09 文件对话框与 URI Spec（基于代码核对 2026-08-17）

## 代码事实
- 已完成：picker 调用（registerFilePicker/filePickerResult）、URI→沙盒拷贝、pendingExports 回拷、fileshare 持久化授权
- **引擎侧跨会话读取 URI（fileAccess.open）：❌ 未实现**（legacy 清单第 4 项）
- **拖拽文件导入（drop files）：❌ 无链路**（与 M03 T-XC-1 同一任务）

## 任务
- [ ] T-FL-1 fileAccess.open 实现引擎侧 URI 读取
- [ ] T-FL-2 对话框 NAPI 桥参数验证（模拟器触发）

## 开放项
- DocumentViewPicker 初始目录/过滤器透传受限。