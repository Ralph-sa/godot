# M19 资源导入 Spec（基于代码核对 2026-08-17）

## 代码事实
- 15 种导入器编入：Texture/Image/LayeredTexture/Atlas/BitMap、SVG、OBJ、Scene、
  OggVorbis、DynamicFont/ImageFont/BMFont、CSVTranslation、ShaderFile、MP
- EditorFileSystem 扫描与 .import 管线在引擎侧完整
- **鸿蒙运行时未验证**：导入器依赖的文件 I/O（沙盒）、线程池、
  图像解码在鸿蒙上的实际行为未测

## 任务
- [ ] T-IM-1 模拟器验证导入管线：向项目目录放入测试资源（png/obj/ogg）
      → 触发扫描 → .import 生成（文件系统级验证，不依赖画面）
- [ ] T-IM-2 纹理导入验证：导入后引擎日志无解码错误（ETC2/ASTC 压缩格式
      在模拟器的支持）

## 开放项
- 模拟器 express_gpu 的压缩纹理格式支持未知。