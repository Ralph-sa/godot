# M17 脚本支持 Spec（基于代码核对 2026-08-17）

## 代码事实
- **GDScript：✅ 完整编入**（gdscript_compiler.cpp + 191 处符号）
- **C#/mono：❌ 未编入**——.so 无 mono_module/GodotSharp/CSharpScript 符号，
  仅有编辑器 UI 文本（语法高亮等）。鸿蒙构建未启用 mono 模块
- 编辑器脚本编辑器（代码高亮/自动补全）依赖 GDScript 模块 ✅

## 任务
- [ ] T-SC-1 评估 mono 模块在鸿蒙的可行性（mono 运行时在 OHOS 的移植工作量
      ——独立大工程：需要 mono 对 musl/鸿蒙沙盒的适配；对照 Android 版 mono 配置）
- [ ] T-SC-2 GDScript 编辑/运行验证：模拟器上编辑脚本文件（文件 I/O 沙盒路径）+ 
      运行 GDScript（场景运行依赖 M18）

## 开放项
- C# 支持是重大决策点：是否列入移植范围需要用户明确（工作量以周计）。