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
// API 26：@ohos 系统模块均以默认导出方式导入
import i18n from '@ohos.i18n';
import deviceInfo from '@ohos.deviceInfo';
import display from '@ohos.display';
// 剪贴板（第 5 轮：编辑器复制/粘贴，@ohos.pasteboard）
import pasteboard from '@ohos.pasteboard';
// 系统文件选择器（第 5 轮：编辑器文件对话框，@ohos.file.picker）
import picker from '@ohos.file.picker';
// 文件授权持久化（第 9 轮：公共目录 URI 持久化授权，@ohos.fileshare）
import fileshare from '@ohos.fileshare';
// 文件系统操作（第 10 轮修复：文件对话框 URI -> 沙盒拷贝，@ohos.file.fs）
import fs from '@ohos.file.fs';
// 窗口模式（第 5 轮：全屏/最大化经 @ohos.window 应用）
import window from '@ohos.window';
// 系统指针（第 7 轮：鼠标捕获/隐藏；第 8 轮：光标形状 setPointerStyle）
import pointer from '@ohos.multimodalInput.pointer';
// 输入设备（第 8 轮：手柄枚举 inputDevice.getDeviceList）
import inputDevice from '@ohos.multimodalInput.inputDevice';

// 屏幕信息回传引擎（updateDisplays 数据载体）
interface DisplayInfo {
  x: number;
  y: number;
  w: number;
  h: number;
  dpi: number;
  refresh: number;
}

// 手柄信息回传引擎（enumerateGamepads 数据载体）
interface GamepadInfo {
  id: number;
  name: string;
  guid: string;
}

@Entry
@Component
struct Index {
  // XComponent 控制器
  private xcomponentController: XComponentController = new XComponentController();
  @State engineStarted: boolean = false;
  // 主窗口 ID（cursor.setPointerStyle 需要；onAppear 时获取）
  private mainWindowId: number = -1;
  // 触控板双指手势跟踪（第 8 轮：双指中心点增量 -> 滚轮注入）
  private touchCenter: Array<number> = [];
  // 待回拷的保存文件映射（第 10 轮修复：沙盒 exports 路径 -> 用户选择的目标 URI），
  // 引擎写完文件后在下一次对话框操作或页面销毁时写回公共目录。
  private pendingExports: Map<string, string> = new Map<string, string>();

  build() {
    Column() {
      // 全屏 XComponent：类型 SURFACE，用于承载 Vulkan 渲染。
      // libraryname 必须指定（本应用的 native 库 libgodot.so -> 'godot'）：
      // 未指定时 onLoad 回调的 context 为 undefined（实测 emulator 7.0.0.23），
      // 拿不到 nativeXComponent -> OHOS_XComponent 无法创建 -> Vulkan surface
      // 创建失败 -> 引擎渲染服务器空指针崩溃。
      XComponent({
        id: 'godot_surface',
        type: XComponentType.SURFACE,
        libraryname: 'godot',
        controller: this.xcomponentController
      })
        .onLoad((context?: object) => {
          // XComponent 加载：把 native 上下文传给 C++（注册输入事件回调）。
          // 部分设备/API 版本 onLoad 回调参数可能为空：兜底从 controller 获取
          //（XComponentController.getXComponentContext() 返回与回调参数相同的上下文）。
          console.info('GodotOHOS onLoad fired, context=' + context);
          // API 26：onLoad 上下文不含 nativeXComponent（实测属性为 undefined），
          // 统一走 surfaceId 路径：native 侧用
          // OH_NativeWindow_CreateNativeWindowFromSurfaceId 直接创建渲染窗口，
          // 不再依赖 OH_NativeXComponent 上下文对象。
          let surfaceId: string = this.xcomponentController.getXComponentSurfaceId();
          godot.setXComponent(surfaceId, this.getScreenWidth(), this.getScreenHeight());
          console.info('GodotOHOS setXComponent surfaceId path: ' + surfaceId);
          // Surface 就绪后启动 Godot 引擎
          if (!this.engineStarted) {
            // 渲染方法：forward_plus 为桌面标准路径；模拟器软件 Vulkan
            // 转译层（express_gpu）在 forward_plus 渲染负载下会自身崩溃
            // （API 24/26 模拟器均复现），模拟器强制用 'mobile'（特性需求
            // 少、更稳定）。真机 2in1 可改回 forward_plus。
            godot.setRenderingMethod('mobile');
            // 初始化资源管理器（rawfile 读取，如导出的 main.pck）
            godot.initResourceManager(getContext(this).resourceManager);
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
          // 页面销毁：写回待保存文件，停止并释放引擎
          this.flushPendingExports();
          godot.stop();
          godot.dispose();
          this.engineStarted = false;
        })
        .onTouch((event: TouchEvent) => {
          // 第 10 轮修复：surfaceId 路径下原生触摸回调不可用，
          // ArkTS 通用触摸事件分两路注入引擎：
          //  - 双指：触控板滚动手势（第 8 轮，注入滚轮事件）；
          //  - 单指：触摸按下/移动/抬起（注入 InputEventScreenTouch/Drag）。
          this.handleTouchGesture(event);
          this.injectTouchEvents(event);
        })
        .onMouse((event: MouseEvent) => {
          // 第 10 轮修复：鼠标事件经 NAPI 注入引擎（原生鼠标回调不可用）。
          // MouseAction: 1=按下 2=抬起 3=移动；MouseButton: 0=左 1=右 2=中。
          let action: number = event.action;
          let button: number = event.button;
          if (event.action != MouseAction.Move) {
            godot.injectMouse(action, event.x, event.y, button);
          } else {
            godot.injectMouse(3, event.x, event.y, button);
          }
          event.stopPropagation();
        })
        .onKeyEvent((event: KeyEvent) => {
          // 第 10 轮修复：键盘事件经 NAPI 注入引擎（原生键盘回调不可用）。
          // KeyType: 0=Down 1=Up；keyCode 为 @ohos.multimodalInput KeyCode。
          let pressed: boolean = (event.type == KeyType.Down);
          godot.injectKey(event.keyCode, pressed);
          event.stopPropagation();
        })
        .onAreaChange((oldArea: Area, newArea: Area) => {
          // 第 10 轮修复：surfaceId 路径无原生 OnSurfaceChanged 回调，
          // 组件尺寸变化（窗口缩放/旋转）经本回调同步引擎（vp -> px）。
          // Area.width/height 为 Length 联合类型，需 Number() 转换后运算。
          let density: number = this.getScreenDensity();
          let w: number = Math.round(Number(newArea.width) * density);
          let h: number = Math.round(Number(newArea.height) * density);
          if (w > 0 && h > 0) {
            godot.injectResize(w, h);
          }
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
          godot.registerFilePicker((title: string, mode: number, suffixes: string) => {
            this.openFilePicker(title, mode, suffixes);
          });
          godot.registerWindowHandler((mode: number) => {
            this.applyWindowMode(mode);
          });
          // 子窗口处理器：引擎请求创建/调整原生子窗口（编辑器子窗口）
          godot.registerSubWindowHandler((op: number, id: number, a: number, b: number, c: number, d: number, title: string) => {
            this.handleSubWindow(op, id, a, b, c, d, title);
          });
          // 指针可见性处理器（鼠标捕获/隐藏）
          godot.registerPointerHandler((visible: boolean) => {
            this.setPointerVisible(visible);
          });
          // 光标形状处理器（第 8 轮：pointer.setPointerStyle）
          godot.registerCursorHandler((shape: number) => {
            this.setCursorShape(shape);
          });
          // 手柄设备枚举处理器（第 8 轮：inputDevice.getDeviceList 过滤 joystick）
          godot.registerGamepadHandler(() => {
            this.enumerateGamepads();
          });
          // 屏幕枚举：查询全部屏幕并回传引擎（多屏支持）
          this.updateDisplays();
          // 主窗口 ID（光标样式设置用）
          this.queryMainWindowId();
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
      return i18n.System.getSystemLanguage();
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
      let defaultDisplay: display.Display = display.getDefaultDisplaySync();
      return defaultDisplay.densityDPI / 160.0;
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

  // 递归拷贝目录 URI 到沙盒（返回拷贝后的沙盒路径）
  private async copyDirToSandbox(srcUri: string, destDir: string): Promise<string> {
    try {
      fs.mkdirSync(destDir, true);
      let entries: Array<string> = fs.listFileSync(srcUri);
      for (let i = 0; i < entries.length; i++) {
        let name: string = entries[i];
        let childUri: string = srcUri + '/' + name;
        let childDest: string = destDir + '/' + name;
        let stat: fs.Stat = fs.statSync(childUri);
        if (stat.isDirectory()) {
          await this.copyDirToSandbox(childUri, childDest);
        } else {
          fs.copyFileSync(childUri, childDest);
        }
      }
      return destDir;
    } catch (e) {
      console.error('GodotOHOS copyDirToSandbox failed: ' + JSON.stringify(e));
      return '';
    }
  }

  // 把用户选择的 URI（文件或目录）拷贝到应用沙盒 imports 目录，
  // 回传沙盒真实路径（引擎 FileAccess 才能打开；公共目录 URI 无法直接访问）。
  private async copySelectionToSandbox(uris: string[]): Promise<string[]> {
    let out: string[] = [];
    let importBase: string = this.getCacheDir() + '/imports';
    try {
      fs.mkdirSync(importBase, true);
    } catch (e) {
      // 目录已存在等场景忽略
    }
    for (let i = 0; i < uris.length; i++) {
      let uri: string = uris[i];
      try {
        let name: string = uri.substring(uri.lastIndexOf('/') + 1);
        if (name.length == 0) {
          name = 'import_' + i;
        }
        // 重名处理：追加序号
        let dest: string = importBase + '/' + name;
        let n: number = 1;
        while (fs.accessSync(dest)) {
          let dot: number = name.lastIndexOf('.');
          let base: string = dot > 0 ? name.substring(0, dot) : name;
          let ext: string = dot > 0 ? name.substring(dot) : '';
          dest = importBase + '/' + base + '_' + n + ext;
          n++;
        }
        let stat: fs.Stat = fs.statSync(uri);
        if (stat.isDirectory()) {
          let r: string = await this.copyDirToSandbox(uri, dest);
          if (r.length > 0) {
            out.push(r);
          }
        } else {
          fs.copyFileSync(uri, dest);
          out.push(dest);
        }
      } catch (e) {
        console.error('GodotOHOS copySelectionToSandbox item failed: ' + JSON.stringify(e));
        // 拷贝失败回退传原始 URI（引擎侧打开会失败，但不丢结果）
        out.push(uri);
      }
    }
    return out;
  }

  // 写回保存映射（引擎已写完的沙盒文件 -> 用户选择的目标 URI）
  private flushPendingExports(): void {
    if (this.pendingExports.size == 0) {
      return;
    }
    let keys: string[] = Array.from(this.pendingExports.keys());
    for (let i = 0; i < keys.length; i++) {
      let sandboxPath: string = keys[i];
      let targetUri: string | undefined = this.pendingExports.get(sandboxPath);
      if (targetUri == undefined) {
        continue;
      }
      try {
        if (fs.accessSync(sandboxPath)) {
          fs.copyFileSync(sandboxPath, targetUri);
        }
      } catch (e) {
        console.error('GodotOHOS flushPendingExports failed: ' + JSON.stringify(e));
      }
      this.pendingExports.delete(sandboxPath);
    }
  }

  // 打开系统文件选择器，选择结果回传 C++（godot.filePickerResult）
  private openFilePicker(title: string, mode: number, suffixes: string): void {
    // 上次保存的待回拷文件先写回目标 URI（引擎已完成写入）
    this.flushPendingExports();
    // FileDialogMode: 0 打开单文件 / 1 打开多文件 / 2 打开目录 / 3 打开任意 / 4 保存
    // 过滤器（第 10 轮修复）：引擎侧逗号分隔后缀串 -> fileSuffixFilters 数组
    let pickerOptions: picker.DocumentSelectOptions = {
      maxSelectNumber: (mode == 1) ? 100 : 1,
    };
    if (suffixes != undefined && suffixes.length > 0) {
      let parts: string[] = suffixes.split(',');
      let valid: string[] = [];
      for (let i = 0; i < parts.length; i++) {
        if (parts[i].length > 0) {
          valid.push(parts[i]);
        }
      }
      if (valid.length > 0) {
        pickerOptions.fileSuffixFilters = valid;
      }
    }
    let documentPicker = new picker.DocumentViewPicker(getContext(this));
    let uriList: string[] = [];
    documentPicker.select(pickerOptions).then((uris: string[]) => {
      uriList = uris;
      // 第 9 轮：持久化授权（公共目录/文档 URI 跨会话可读）。
      // fileshare.persistPermission 使所选 URI 授权在应用重启后仍有效，
      // 否则仅当次会话有效（对应 macOS security-scoped bookmarks 语义）。
      // API 26：PolicyInfo 为 { uri, operationMode }，读权限取 OperationMode.READ_MODE
      let policies: Array<fileshare.PolicyInfo> = [];
      for (let i = 0; i < uriList.length; i++) {
        policies.push({
          uri: uriList[i],
          operationMode: fileshare.OperationMode.READ_MODE,
        });
      }
      if (policies.length > 0) {
        fileshare.persistPermission(policies).catch(() => {
          // 授权失败不阻塞：本次会话内仍可读取
        });
      }
    }).catch(() => {
      uriList = [];
    }).finally(() => {
      // 第 10 轮修复：公共目录 URI（file://docs/...）引擎无法直接打开，
      // 拷贝到沙盒 imports 目录后回传沙盒路径。保存模式（4）回传沙盒
      // exports 路径供引擎写入，并记录「沙盒路径 -> 目标 URI」映射，
      // 由 flushPendingExports（下次对话框操作/页面销毁）写回公共目录。
      if (uriList.length > 0 && mode != 4) {
        this.copySelectionToSandbox(uriList).then((sandboxPaths: string[]) => {
          godot.filePickerResult(JSON.stringify(sandboxPaths));
        });
      } else if (uriList.length > 0 && mode == 4) {
        let exportBase: string = this.getCacheDir() + '/exports';
        try {
          fs.mkdirSync(exportBase, true);
        } catch (e) {
          // 已存在忽略
        }
        let exportPaths: string[] = [];
        for (let i = 0; i < uriList.length; i++) {
          let uri: string = uriList[i];
          let name: string = uri.substring(uri.lastIndexOf('/') + 1);
          if (name.length == 0) {
            name = 'export_' + i;
          }
          let sandboxPath: string = exportBase + '/' + name;
          this.pendingExports.set(sandboxPath, uri);
          exportPaths.push(sandboxPath);
        }
        godot.filePickerResult(JSON.stringify(exportPaths));
      } else {
        godot.filePickerResult(JSON.stringify(uriList));
      }
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
        win.setWindowTopmost(true);
      } else {
        // 恢复正常窗口（退出全屏/最大化/取消置顶）
        win.setWindowLayoutFullScreen(false);
        win.setWindowTopmost(false);
      }
    } catch (e) {
      // 窗口操作失败：忽略（引擎内窗口状态已记录）
    }
  }

  // ---- 屏幕枚举（第 6 轮，@ohos.display getAllDisplays） ----

  // 查询全部屏幕（位置/尺寸/DPI/刷新率）并回传引擎
  private updateDisplays(): void {
    let displayList: Array<DisplayInfo> = [];
    display.getAllDisplays().then((displays: Array<display.Display>) => {
      for (let i = 0; i < displays.length; i++) {
        let d: display.Display = displays[i];
        let info: DisplayInfo = {
          x: 0,
          y: 0,
          w: d.width,
          h: d.height,
          dpi: d.densityDPI,
          refresh: d.refreshRate
        };
        displayList.push(info);
      }
    }).catch(() => {
      // 查询失败：引擎保持单屏默认
    }).finally(() => {
      godot.updateDisplays(JSON.stringify(displayList));
    });
  }

  // ---- 子窗口（第 7 轮，@ohos.window createWindow） ----

  // 引擎请求的原生子窗口操作：0 创建 / 1 销毁 / 2 设标题 / 3 设位置尺寸 / 4 设可见
  private handleSubWindow(op: number, id: number, a: number, b: number, c: number, d: number, title: string): void {
    let key: string = 'godot_sub_' + id;
    try {
      if (op == 0) {
        // 创建原生子窗口（编辑器子窗口：弹窗/工具面板）
        // API 26：createWindow(config) 只需传入 Configuration（含 name/windowType/ctx）
        let config: window.Configuration = {
          name: key,
          windowType: window.WindowType.TYPE_FLOAT,
          ctx: getContext(this)
        };
        window.createWindow(config).then((win: window.Window) => {
          win.moveWindowTo(a, b);
          win.resize(c, d);
        });
      } else if (op == 1) {
        // findWindow 为同步 API：直接取窗口实例后销毁
        let win = window.findWindow(key);
        win.destroyWindow();
      } else if (op == 2) {
        let win = window.findWindow(key);
        win.setWindowTitle(title);
      } else if (op == 3) {
        let win = window.findWindow(key);
        win.moveWindowTo(a, b);
        win.resize(c, d);
      } else if (op == 4) {
        let win = window.findWindow(key);
        if (a == 1) {
          win.showWindow();
        } else {
          // API 26：子窗口隐藏使用 minimize()（对子窗口语义为隐藏）
          win.minimize();
        }
      }
    } catch (e) {
      // 子窗口操作失败：忽略（引擎内窗口状态已记录）
    }
  }

  // ---- 指针可见性（第 7 轮，@ohos.multimodalInput.pointer） ----

  // 设置系统指针可见性（鼠标捕获/隐藏时调用）
  private setPointerVisible(visible: boolean): void {
    try {
      pointer.setPointerVisible(visible);
    } catch (e) {
      // 设置失败：忽略
    }
  }

  // ---- 光标形状（第 8 轮，@ohos.multimodalInput.pointer.setPointerStyle） ----

  // 查询主窗口 ID（pointer.setPointerStyle 需要）
  private queryMainWindowId(): void {
    try {
      window.getLastWindow(getContext(this)).then((win: window.Window) => {
        this.mainWindowId = win.getWindowProperties().id;
      }).catch(() => {
        this.mainWindowId = -1;
      });
    } catch (e) {
      this.mainWindowId = -1;
    }
  }

  // Godot CursorShape -> OHOS PointerStyle 映射（编辑器系统光标）
  private cursorShapeMap: Map<number, number> = new Map<number, number>([
    [0, pointer.PointerStyle.DEFAULT],            // CURSOR_ARROW -> 默认箭头
    [1, pointer.PointerStyle.TEXT_CURSOR],        // CURSOR_IBEAM -> 文本光标
    [2, pointer.PointerStyle.HAND_POINTING],      // CURSOR_POINTING_HAND -> 手型
    [3, pointer.PointerStyle.CROSS],              // CURSOR_CROSS -> 十字
    [4, pointer.PointerStyle.DEFAULT],            // CURSOR_WAIT -> 默认（无等待动画）
    [5, pointer.PointerStyle.DEFAULT],            // CURSOR_BUSY
    [6, pointer.PointerStyle.MOVE],               // CURSOR_DRAG -> 移动
    [7, pointer.PointerStyle.HAND_OPEN],          // CURSOR_CAN_DROP -> 张开手
    [8, pointer.PointerStyle.CURSOR_FORBID],      // CURSOR_FORBIDDEN -> 禁止
    [9, pointer.PointerStyle.RESIZE_UP_DOWN],     // CURSOR_VSIZE -> 垂直缩放
    [10, pointer.PointerStyle.RESIZE_LEFT_RIGHT], // CURSOR_HSIZE -> 水平缩放
    [11, pointer.PointerStyle.NORTH_EAST_SOUTH_WEST], // CURSOR_BDIAGSIZE
    [12, pointer.PointerStyle.NORTH_WEST_SOUTH_EAST], // CURSOR_FDIAGSIZE
    [13, pointer.PointerStyle.MOVE],              // CURSOR_MOVE
    [14, pointer.PointerStyle.RESIZE_UP_DOWN],    // CURSOR_VSPLIT
    [15, pointer.PointerStyle.RESIZE_LEFT_RIGHT], // CURSOR_HSPLIT
    [16, pointer.PointerStyle.HELP],              // CURSOR_HELP
  ]);

  // 切换系统光标形状（引擎 cursor_set_shape 回调）
  private setCursorShape(shape: number): void {
    if (this.mainWindowId < 0) {
      return; // 窗口 ID 未就绪
    }
    let style = this.cursorShapeMap.get(shape);
    if (style == undefined) {
      style = pointer.PointerStyle.DEFAULT;
    }
    try {
      pointer.setPointerStyle(this.mainWindowId, style);
    } catch (e) {
      // 光标设置失败：忽略
    }
  }

  // ---- 单指触摸注入（第 10 轮修复：原生触摸回调不可用，ArkTS 事件桥） ----
  // TouchType: 0=Down 1=Up 2=Move 3=Cancel；单指时逐触点注入引擎。
  private injectTouchEvents(event: TouchEvent): void {
    let touches = event.touches;
    if (touches.length != 1) {
      return; // 双指交给滚动手势，多点暂不支持
    }
    let t = touches[0];
    let type: number = t.type;
    if (type == 3) {
      type = 2; // Cancel 按抬起处理
    }
    godot.injectTouch(type, t.id, t.x, t.y);
  }

  // ---- 触控板手势（第 8 轮，双指滑动 -> 滚轮注入） ----

  // 双指中心点增量识别为滚轮事件（触控板滚动；单指触摸仍由引擎处理）
  private handleTouchGesture(event: TouchEvent): void {
    let touches = event.touches;
    if (touches.length == 2) {
      let cx = (touches[0].x + touches[1].x) / 2.0;
      let cy = (touches[0].y + touches[1].y) / 2.0;
      if (this.touchCenter.length == 2) {
        let dx = cx - this.touchCenter[0];
        let dy = cy - this.touchCenter[1];
        // 阈值过滤抖动；增量缩放为滚轮格数（真机联调可调整系数）
        if (Math.abs(dx) > 0.8 || Math.abs(dy) > 0.8) {
          godot.injectWheel(dx * 0.5, dy * 0.5);
        }
      }
      this.touchCenter = [cx, cy];
    } else {
      this.touchCenter = [];
    }
  }

  // ---- 手柄设备枚举（第 8 轮，@ohos.multimodalInput.inputDevice） ----

  // 枚举全部输入设备，过滤 joystick（手柄）后回传引擎
  private enumerateGamepads(): void {
    let deviceList: Array<GamepadInfo> = [];
    inputDevice.getDeviceList().then((ids: Array<number>) => {
      let jobs: Array<Promise<void>> = [];
      for (let i = 0; i < ids.length; i++) {
        let deviceId = ids[i];
        jobs.push(new Promise<void>((resolve) => {
          inputDevice.getDeviceInfo(deviceId).then((info: inputDevice.InputDeviceData) => {
            // SourceType 为字符串类型：设备能力数组 sources 含 'joystick' 即为手柄
            let isJoystick = info.sources != undefined && info.sources.indexOf('joystick') >= 0;
            if (isJoystick) {
              let gpad: GamepadInfo = {
                id: deviceId,
                name: info.name,
                guid: 'ohos-' + deviceId + '-' + info.name.replace(/[^A-Za-z0-9]/g, '')
              };
              deviceList.push(gpad);
            }
            resolve();
          }).catch(() => {
            resolve();
          });
        }));
      }
      Promise.all(jobs).then(() => {
        godot.gamepadDevices(JSON.stringify(deviceList));
      });
    }).catch(() => {
      // 枚举失败：引擎保持无手柄状态
      godot.gamepadDevices('[]');
    });
  }
}
