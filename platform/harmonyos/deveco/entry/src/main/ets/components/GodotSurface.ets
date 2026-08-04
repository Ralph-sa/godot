/**
 * GodotSurface.ets - XComponent wrapper for Godot Vulkan rendering
 *
 * Encapsulates an XComponent that serves as the rendering surface
 * for Godot's Vulkan renderer. Forwards keyboard, mouse, and touch
 * input events to the NAPI bridge, and manages the XComponent lifecycle.
 */
import { hilog } from '@kit.PerformanceAnalysisKit';
import { inputMethod } from '@kit.IMEKit';
import { godot_napi } from 'libgodot_napi.so';

const TAG = 'GodotSurface';
const DOMAIN = 0x0000;
let nextSurfaceGeneration: number = 1;

export type SurfaceReadyCallback =
  (surfaceId: string, width: number, height: number, generation: number) => void;

@ComponentV2
export struct GodotSurface {
  @Param @Require onSurfaceReady?: SurfaceReadyCallback;
  @Local surfaceId: string = '';
  @Local surfaceReadyReported: boolean = false;
  @Local surfaceGeneration: number = 0;
  private surfaceDestroyReported: boolean = false;
  private nativeXComponentReady: boolean = false;
  private reportedSurfaceId: string = '';
  private reportedSurfaceWidth: number = 0;
  private reportedSurfaceHeight: number = 0;
  private xcController: XComponentController = new XComponentController();
  private imeController?: inputMethod.InputMethodController;
  private imeCallbacksRegistered: boolean = false;
  private lastMouseX: number = 0;
  private lastMouseY: number = 0;

  aboutToAppear(): void {
    let generation = nextSurfaceGeneration;
    nextSurfaceGeneration++;
    this.surfaceId = 'godot_surface_' + generation.toString();
    this.surfaceGeneration = generation;
    this.surfaceReadyReported = false;
    this.surfaceDestroyReported = false;
    this.nativeXComponentReady = false;
    this.reportedSurfaceId = '';
    this.reportedSurfaceWidth = 0;
    this.reportedSurfaceHeight = 0;
    hilog.info(DOMAIN, TAG, '[INIT STEP 15/16] GodotSurface aboutToAppear, id: ' + this.surfaceId);
  }

  private reportSurfaceDestroyed(): void {
    if (this.surfaceDestroyReported) {
      return;
    }

    this.surfaceDestroyReported = true;
    this.nativeXComponentReady = false;
    this.releaseIme();
    hilog.info(DOMAIN, TAG, 'XComponent surface destroyed: ' + this.surfaceId);
    try {
      if (this.reportedSurfaceId.length > 0 && this.surfaceGeneration > 0) {
        godot_napi.onSurfaceDestroy(this.reportedSurfaceId, this.surfaceGeneration);
      }
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'NAPI surface destroy error: ' + e);
    }
  }

  private imeInsertText = (text: string): void => {
    if (text.length === 0) {
      return;
    }
    try {
      godot_napi.sendImeUpdate('', 0, 0);
      godot_napi.sendInputText(text);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'IME insertText forwarding failed: ' + e);
    }
  };

  private imeDeleteLeft = (length: number): void => {
    let count = Math.min(Math.max(length, 0), 64);
    for (let index = 0; index < count; index++) {
      godot_napi.sendKeyEvent(2055, 0, '');
      godot_napi.sendKeyEvent(2055, 1, '');
    }
  };

  private imeDeleteRight = (length: number): void => {
    let count = Math.min(Math.max(length, 0), 64);
    for (let index = 0; index < count; index++) {
      godot_napi.sendKeyEvent(2071, 0, '');
      godot_napi.sendKeyEvent(2071, 1, '');
    }
  };

  private imePreviewText = (text: string, range: inputMethod.Range): void => {
    let selectionLength = Math.max(0, range.end - range.start);
    godot_napi.sendImeUpdate(text, Math.max(0, range.start), selectionLength);
  };

  private imePreviewFinished = (): void => {
    godot_napi.sendImeUpdate('', 0, 0);
  };

  private registerImeCallbacks(controller: inputMethod.InputMethodController): void {
    if (this.imeCallbacksRegistered) {
      return;
    }
    controller.on('insertText', this.imeInsertText);
    controller.on('deleteLeft', this.imeDeleteLeft);
    controller.on('deleteRight', this.imeDeleteRight);
    controller.on('getLeftTextOfCursor', (_length: number): string => '');
    controller.on('getRightTextOfCursor', (_length: number): string => '');
    controller.on('getTextIndexAtCursor', (): number => 0);
    controller.on('setPreviewText', this.imePreviewText);
    controller.on('finishTextPreview', this.imePreviewFinished);
    this.imeCallbacksRegistered = true;
  }

  private async attachIme(): Promise<void> {
    try {
      if (!this.imeController) {
        this.imeController = inputMethod.getController();
      }
      let controller = this.imeController;
      this.registerImeCallbacks(controller);
      let config: inputMethod.TextConfig = {
        inputAttribute: {
          textInputType: inputMethod.TextInputType.TEXT,
          enterKeyType: inputMethod.EnterKeyType.NEWLINE
        },
        cursorInfo: { left: 0, top: 0, width: 1, height: 1 },
        selection: { start: 0, end: 0 },
        newEditBox: true
      };
      await controller.attach(false, config);
      hilog.info(DOMAIN, TAG, 'IME attached to focused Godot surface');
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'IME attach unavailable: ' + e);
    }
  }

  private async detachIme(): Promise<void> {
    if (!this.imeController) {
      return;
    }
    try {
      await this.imeController.detach();
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'IME detach failed: ' + e);
    }
  }

  private releaseIme(): void {
    if (!this.imeController) {
      return;
    }
    try {
      this.imeController.off('insertText', this.imeInsertText);
      this.imeController.off('deleteLeft', this.imeDeleteLeft);
      this.imeController.off('deleteRight', this.imeDeleteRight);
      this.imeController.off('setPreviewText', this.imePreviewText);
      this.imeController.off('finishTextPreview', this.imePreviewFinished);
      this.detachIme();
    } catch (e) {
      hilog.warn(DOMAIN, TAG, 'IME release failed: ' + e);
    }
    this.imeCallbacksRegistered = false;
    this.imeController = undefined;
  }

  /**
   * Reports a valid physical-pixel surface identity and every later resize.
   * Main::setup is not allowed to begin before the first report succeeds.
   */
  private reportSurfaceReady(): void {
    if (this.surfaceDestroyReported) {
      return;
    }
    if (!this.nativeXComponentReady) {
      hilog.warn(DOMAIN, TAG, 'Surface ready deferred until native XComponent context is captured');
      return;
    }

    try {
      const sid: string = this.xcController.getXComponentSurfaceId();
      // getXComponentSurfaceRect() has no exported ArkTS type, so read its
      // fields directly instead of declaring an intermediate value.
      const surfW: number = Math.round(this.xcController.getXComponentSurfaceRect().surfaceWidth);
      const surfH: number = Math.round(this.xcController.getXComponentSurfaceRect().surfaceHeight);
      if (sid.length === 0 || surfW <= 0 || surfH <= 0) {
        hilog.warn(DOMAIN, TAG, `Surface metadata not ready: id=${sid} rect=${surfW}x${surfH}`);
        return;
      }

      if (!this.onSurfaceReady) {
        hilog.error(DOMAIN, TAG, 'Required onSurfaceReady callback is missing');
        return;
      }

      if (this.surfaceReadyReported && sid === this.reportedSurfaceId &&
        surfW === this.reportedSurfaceWidth && surfH === this.reportedSurfaceHeight) {
        return;
      }

      this.surfaceReadyReported = true;
      this.reportedSurfaceId = sid;
      this.reportedSurfaceWidth = surfW;
      this.reportedSurfaceHeight = surfH;
      hilog.info(DOMAIN, TAG,
        `[INIT STEP 4/17] surface ready: ${sid} generation=${this.surfaceGeneration} ${surfW}x${surfH}`);
      this.onSurfaceReady(sid, surfW, surfH, this.surfaceGeneration);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'Failed to query XComponent surface metadata: ' + e);
    }
  }

  // ---- Keyboard Event Forwarding ----
  //
  // OHOS key events arrive as onKeyEvent on the XComponent.
  // type: KeyType.Down=0, KeyType.Up=1
  // keyCode: OHOS key code (e.g. 2070=Escape, 2012=DpadUp)
  // keyText: text representation
  handleKeyEvent(event: KeyEvent): void {
    try {
      godot_napi.sendKeyEvent(event.keyCode, event.type === KeyType.Down ? 0 : 1,
        event.keyText ?? '');
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'Key event error: ' + e);
    }
  }

  // ---- Mouse Event Forwarding ----
  //
  // OHOS mouse events: onMouse on the XComponent.
  // MouseButton is a platform enum/bit mask, while the C++ bridge expects
  // 0=Left, 1=Middle, 2=Right, 3=Back, 4=Forward.
  // MouseEvent coordinates are vp; Godot and the XComponent surface use px.
  private mapMouseButton(button: MouseButton): number {
    switch (button) {
      case MouseButton.Left:
        return 0;
      case MouseButton.Middle:
        return 1;
      case MouseButton.Right:
        return 2;
      case MouseButton.Back:
        return 3;
      case MouseButton.Forward:
        return 4;
      default:
        return -1;
    }
  }

  handleMouseEvent(event: MouseEvent): void {
    const uiContext = this.getUIContext();
    const x: number = uiContext.vp2px(event.x ?? 0);
    const y: number = uiContext.vp2px(event.y ?? 0);
    const offsetX: number = x - this.lastMouseX;
    const offsetY: number = y - this.lastMouseY;
    this.lastMouseX = x;
    this.lastMouseY = y;

    let action: number;

    switch (event.action) {
      case MouseAction.Press:
        action = 0; // button press
        break;
      case MouseAction.Release:
        action = 1; // button release
        break;
      case MouseAction.Move:
        action = 2; // move
        break;
      default:
        action = 2;
        break;
    }

    const button: number = this.mapMouseButton(event.button ?? MouseButton.None);
    if (action !== 2 && button < 0) {
      hilog.warn(DOMAIN, TAG, 'Ignoring unsupported mouse button: ' + event.button);
      return;
    }

    try {
      // Motion does not consume the button index; use left as a neutral value
      // when HarmonyOS reports MouseButton.None.
      godot_napi.sendMouseEvent(button < 0 ? 0 : button, action, x, y, offsetX, offsetY);
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'Mouse event error: ' + e);
    }
  }

  // ---- Touch Event Forwarding ----
  //
  // OHOS touch events arrive as onTouch on the XComponent.
  // TouchType.Down=0, Up=1, Move=2
  handleTouchEvent(event: TouchEvent): void {
    const touches: TouchObject[] = event.touches;
    if (!touches || touches.length === 0) {
      return;
    }

    const uiContext = this.getUIContext();
    for (let i = 0; i < touches.length; i++) {
      const t: TouchObject = touches[i];
      let action: number;

      switch (t.type) {
        case TouchType.Down:
          action = 0;
          break;
        case TouchType.Up:
          action = 1;
          break;
        case TouchType.Move:
          action = 2;
          break;
        default:
          continue;
      }

      try {
        godot_napi.sendTouchEvent(t.id, action, uiContext.vp2px(t.x), uiContext.vp2px(t.y));
      } catch (e) {
        hilog.error(DOMAIN, TAG, 'Touch event error: ' + e);
      }
    }

    // Prevent default handling to avoid conflicts
    event.stopPropagation();
  }

  build() {
    Stack({ alignContent: Alignment.TopStart }) {
      XComponent({
        id: this.surfaceId,
        type: XComponentType.SURFACE,
        libraryname: 'godot_napi',
        controller: this.xcController
      })
        .width('100%')
        .height('100%')
        .backgroundColor('#1E1E2E')
        .focusable(true)
        .focusOnTouch(true)
        .onLoad((context?: object) => {
          hilog.info(DOMAIN, TAG, '[INIT STEP 3/17] XComponent onLoad: ' + this.surfaceId);
          if (!context) {
            hilog.error(DOMAIN, TAG, 'XComponent onLoad did not provide a native context');
            return;
          }
          let inputInitResult: number = godot_napi.initXComponent(context);
          hilog.info(DOMAIN, TAG, 'Native XComponent input registration result: ' + inputInitResult);
          if (inputInitResult !== 0) {
            this.nativeXComponentReady = false;
            hilog.error(DOMAIN, TAG, 'Native XComponent registration failed; engine startup deferred');
            return;
          }
          this.nativeXComponentReady = true;
          this.reportSurfaceReady();
        })
        .onAreaChange((_oldValue: Area, newValue: Area) => {
          hilog.info(DOMAIN, TAG, 'XComponent area: w=' + newValue.width + ' h=' + newValue.height);
          // Some devices report a zero rectangle during onLoad and publish the
          // real size in the first area change. Retry until one valid report.
          this.reportSurfaceReady();
        })
        .onDestroy(() => {
          this.reportSurfaceDestroyed();
        })
        .onKeyEvent((event: KeyEvent) => {
          this.handleKeyEvent(event);
        })
        .onFocus(() => {
          this.attachIme();
        })
        .onBlur(() => {
          this.detachIme();
        })
        .onMouse((event: MouseEvent) => {
          this.handleMouseEvent(event);
        })
        .onTouch((event: TouchEvent) => {
          this.handleTouchEvent(event);
        })
    }
    .width('100%')
    .height('100%')
  }
}
