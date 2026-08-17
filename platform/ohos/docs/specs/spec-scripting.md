# M17 脚本支持 Spec（基于代码核对 2026-08-17）

## 代码事实
- **GDScript：✅ 完整编入**（gdscript_compiler.cpp + 191 处符号）
- **C#/mono：❌ 未编入**——.so 无 mono_module/GodotSharp/CSharpScript 符号，
  仅有编辑器 UI 文本（语法高亮等）。鸿蒙构建未启用 mono 模块
- 编辑器脚本编辑器（代码高亮/自动补全）依赖 GDScript 模块 ✅

## 决策（2026-08-17 用户拍板）
- **C#/mono 不移植**——明确排除，文档标注「不支持」。编辑器 C# 相关 UI
  保持禁用状态（引擎侧 mono 未编入即为现状，无额外工作）。

## 任务
- [ ] T-SC-1 文档标注：TESTING.md 与 legacy 清单记录「C# 不支持（决策排除）」
- [ ] T-SC-2 GDScript 编辑/运行验证：模拟器上编辑脚本文件（文件 I/O 沙盒路径）+
      运行 GDScript（场景运行依赖 M18）

## 开放项
- 无（C# 已排除）。