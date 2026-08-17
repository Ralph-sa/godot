# M01 OS 层 Spec

## 目标
Godot 在鸿蒙上的操作系统抽象：路径、工作目录、进程生命周期、进程内重启。

## 验收标准
- ① sandbox files/cache 目录注入正确（ArkTS 传入，NAPI 解析）
- ② set_cwd/get_cwd 可用（chdir 项目目录——--path 依赖）
- ③ get_resource_dir 返回空（否则 ProjectSettings::_setup 走 res:// 分支失败回退 PM）
- ④ create_instance 进程内重启生效（编辑器打开项目不卡）
- ⑤ 验证：T01 启动链 PASS（模拟器）

## 状态
✅ 完成——上述验收全部通过（T01 PASS；globals->setup OK 打点实证）。

## 开放项
- 无。