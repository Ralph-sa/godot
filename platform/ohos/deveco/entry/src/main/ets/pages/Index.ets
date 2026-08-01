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
// 剪贴板（第 5 轮：编辑器复制/粘贴，@ohos.pasteboard）
import { pasteboard } from '@ohos.pasteboard';
// 系统文件选择器（第 5 轮：编辑器文件对话框，@ohos.file.picker）
import { picker } from '@ohos.file.picker';
// 窗口模式（第 5 轮：全屏/最大化经 @ohos.window 应用）
import { window } from '@ohos.window';

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
        .onAppear(() => {
          // 页面出现：注册剪贴板回调 + 文件选择器处理（@ohos.pasteboard / @ohos.file.picker）
          godot.registerClipboard(
            (text: string) => {
              this.setClipboardText(text);
            },
            (): string => {
              return this.getClipboardText();
            });
          godot.registerFilePicker((title: string, mode: number) => {
            this.openFilePicker(title, mode);
          });
          godot.registerWindowHandler((mode: number) => {
            this.applyWindowMode(mode);
          });
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

  // ---- 剪贴板（第 5 轮，@ohos.pasteboard） ----

  // 写剪贴板：设置纯文本 PasteData
  private setClipboardText(text: string): void {
    try {
      let data = pasteboard.createData(pasteboard.MIMETYPE_TEXT_PLAIN, text);
      pasteboard.getSystemPasteboard().setData(data);
    } catch (e) {
      // 剪贴板被占用等场景：忽略
    }
  }

  // 读剪贴板：返回纯文本内容（无文本返回空串）
  private getClipboardText(): string {
    try {
      let data = pasteboard.getSystemPasteboard().getDataSync();
      if (data.getPrimaryText() != undefined) {
        return data.getPrimaryText() as string;
      }
    } catch (e) {
      // 无数据/未授权：返回空串
    }
    return '';
  }

  // ---- 文件选择器（第 5 轮，@ohos.file.picker DocumentViewPicker） ----

  // 打开系统文件选择器，选择结果回传 C++（godot.filePickerResult）
  private openFilePicker(title: string, mode: number): void {
    // FileDialogMode: 0 打开单文件 / 1 打开多文件 / 2 打开目录 / 3 打开任意 / 4 保存
    let pickerOptions: picker.DocumentViewPickerOptions = {
      maxSelectNumber: (mode == 1) ? 100 : 1,
    };
    let documentPicker = new picker.DocumentViewPicker(getContext(this));
    let uriList: string[] = [];
    documentPicker.select(pickerOptions).then((uris: string[]) => {
      uriList = uris;
    }).catch(() => {
      uriList = [];
    }).finally(() => {
      // 回传结果（JSON 数组字符串）；用户取消时为空数组
      godot.filePickerResult(JSON.stringify(uriList));
    });
  }

  // ---- 窗口模式（第 5 轮，@ohos.window） ----

  // 应用窗口模式：0 窗口化 / 2 最大化 / 3 全屏 / 1001 置顶
  private async applyWindowMode(mode: number): Promise<void> {
    try {
      let win = await window.getLastWindow(getContext(this));
      if (mode == 2) {
        win.maximize();
      } else if (mode == 3) {
        win.setWindowLayoutFullScreen(true);
      } else if (mode == 1001) {
        win.setWindowTopMost(true);
      } else {
        // 恢复正常窗口（退出全屏/最大化/取消置顶）
        win.setWindowLayoutFullScreen(false);
        win.setWindowTopMost(false);
      }
    } catch (e) {
      // 窗口操作失败：忽略（引擎内窗口状态已记录）
    }
  }
}
