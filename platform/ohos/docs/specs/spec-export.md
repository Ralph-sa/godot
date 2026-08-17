# M12 导出器 Spec（基于代码核对 2026-08-17）

## 代码事实（修正旧 spec 的"骨架"判断）
- export_plugin.cpp 263 行完整实现：get_platform_features/get_preset_features/
  get_export_options/has_valid_export_configuration/has_valid_project_configuration/
  export_project 全部有实体——**是完整实现而非骨架**
- export.cpp 56 行（注册）

## 任务
- [ ] T-EX-1 导出模板补全核对：读 export_project 实体确认模板文件清单是否齐全（图标/隐私声明）
- [ ] T-EX-2 验证：模拟器上引擎触发导出 → 产物目录可被 build_hap.sh 构建

## 开放项
- 导出目标设备类型/横竖屏配置待用户输入。