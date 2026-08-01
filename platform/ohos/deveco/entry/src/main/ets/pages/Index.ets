/*
 * Index.ets —— Godot 编辑器主页（ArkTS）
 *
 * 对应 macOS 编辑器的主窗口：鸿蒙侧提供一个全屏 XComponent(SURFACE)，
 * Godot 引擎在其上通过 Vulkan 自绘整个编辑器 UI。
 *
 * 生命周期：
 *  - onWindowStageCreate 时启动引擎（本骨架由 UIAbility 调起）；
 *  - XComponent Surface 就绪后 C++ 侧自动创建 Vulkan Surface；
 *  - XComponent onLoad 时把 nativeXComponent 上下文传给 C++（setXComponent），
 *    C++ 侧注册触摸/鼠标/键盘事件回调。
 *
 * 第 2 轮（功能深化）：接入 setXComponent，并注入系统语言/型号/屏幕密度。
 */

// 导入 C++ 侧 NAPI 模块（moduleName = "godot"，见 native 模块 CMake）
import godot from 'libgodot.so';

// 系统能力：语言/设备/显示信息（第 4 轮联调注入）
import { System as i18nSystem } from '@ohos.i18n';
import { deviceInfo } from '@ohos.deviceInfo';
import { display } from '@ohos.display';

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
        .onLoad((context?: object) => {
          // XComponent 加载：把 native 上下文传给 C++（注册输入事件回调）
          if (context != undefined) {
            godot.setXComponent(context);
          }
          // Surface 就绪后启动 Godot 引擎
          if (!this.engineStarted) {
            godot.initialize(this.getFilesDir(), this.getCacheDir(),
              this.getSystemLanguage(), this.getModelName(), this.getScreenDensity(), this.getRefreshRate());
            godot.start(this.getScreenWidth(), this.getScreenHeight());
            this.engineStarted = true;
          }
        })
        .onBlur(() => {
          // 窗口失焦：通知引擎（XComponent focus 回调仅报告获得焦点）
          godot.notifyFocus(false);
        })
        .onFocus(() => {
          // 窗口获得焦点
          godot.notifyFocus(true);
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

  // 系统语言（@ohos.i18n）——用于 Godot 编辑器本地化
  private getSystemLanguage(): string {
    try {
      return i18nSystem.getSystemLanguage();
    } catch (e) {
      return 'en';
    }
  }

  // 设备型号（@ohos.deviceInfo）
  private getModelName(): string {
    try {
      return deviceInfo.productModel;
    } catch (e) {
      return 'HarmonyOS Device';
    }
  }

  // 屏幕密度（@ohos.display densityDPI / 160）
  private getScreenDensity(): number {
    try {
      let display = display.getDefaultDisplaySync();
      return display.densityDPI / 160.0;
    } catch (e) {
      return 1.0;
    }
  }

  // 屏幕刷新率（@ohos.display refreshRate）
  private getRefreshRate(): number {
    try {
      return display.getDefaultDisplaySync().refreshRate;
    } catch (e) {
      return 60.0;
    }
  }

  // 屏幕宽度（vp -> px 换算由引擎侧 density 处理）
  private getScreenWidth(): number {
    try {
      return display.getDefaultDisplaySync().width;
    } catch (e) {
      return 1920;
    }
  }

  // 屏幕高度
  private getScreenHeight(): number {
    try {
      return display.getDefaultDisplaySync().height;
    } catch (e) {
      return 1080;
    }
  }
}
