/*
 * Index.ets —— Godot 编辑器主页（ArkTS）
 *
 * 对应 macOS 编辑器的主窗口：鸿蒙侧提供一个全屏 XComponent(SURFACE)，
 * Godot 引擎在其上通过 Vulkan 自绘整个编辑器 UI。
 *
 * 生命周期：
 *  - onWindowStageCreate 时启动引擎（本骨架由 UIAbility 调起）；
 *  - XComponent Surface 就绪后 C++ 侧自动创建 Vulkan Surface。
 *
 * 第 1 轮（骨架期）：仅承载 XComponent 与 NAPI 初始化/启动调用；
 * 输入事件分发（OnDispatchTouchEvent / 键盘）在完善期接入。
 */

// 导入 C++ 侧 NAPI 模块（moduleName = "godot"，见 native 模块 CMake）
import godot from 'libgodot.so';

@Entry
@Component
struct Index {
  // XComponent 控制器
  private xcomponentController: XComponentController = new XComponentController();
  @State engineStarted: boolean = false;

  build() {
    Column() {
      // 全屏 XComponent：类型 SURFACE，用于承载 Vulkan 渲染
      XComponent({
        id: 'godot_surface',
        type: XComponentType.SURFACE,
        controller: this.xcomponentController
      })
        .onLoad(() => {
          // Surface 创建回调：启动 Godot 引擎
          if (!this.engineStarted) {
            godot.initialize(this.getFilesDir(), this.getCacheDir());
            godot.start(1920, 1080);
            this.engineStarted = true;
          }
        })
        .onDestroy(() => {
          // 页面销毁：停止并释放引擎
          godot.stop();
          godot.dispose();
          this.engineStarted = false;
        })
        .width('100%')
        .height('100%')
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Color.Black)
  }

  // 获取应用沙盒 files 目录（鸿蒙 Stage 模型）
  private getFilesDir(): string {
    return getContext(this).filesDir;
  }

  // 获取应用沙盒 cache 目录
  private getCacheDir(): string {
    return getContext(this).cacheDir;
  }
}
