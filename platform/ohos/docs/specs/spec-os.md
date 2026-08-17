# M01 OS 层 Spec（基于代码核对 2026-08-17）

## 代码事实
- set_cwd/get_cwd：✅ 已实现（chdir + getcwd）
- get_resource_dir：✅ 返回空（修复）
- create_instance 进程内重启：✅（os_ohos.cpp create_instance 分支）
- get_unique_id：⚠️ 回退 MAC 地址方案（os_ohos.cpp:289）
- **shell_open / open_url：❌ 空壳返回 ERR_UNAVAILABLE（os_ohos.cpp:293-297）**
  ——影响：编辑器帮助菜单打开文档、AssetLib、打开项目文件夹等全部不可用
- get_system_ca_certificates：⚠️ 需核实（299 行起）

## 任务
- [ ] T-OS-1 shell_open 实现：NAPI 桥接 ArkTS 侧打开 URI（startAbility 或浏览器）
- [ ] T-OS-2 get_unique_id 评估：设备标识 API 可用性（对照 openharmony-src）
- [ ] T-OS-3 核实 get_system_ca_certificates 返回内容（mbedtls 证书错误 ERR -8576 是否相关）

## 开放项
- shell_open 的鸿蒙对应能力（@ohos.abilityAccessCtrl? startAbility? browser）需源码确认。