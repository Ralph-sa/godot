/**
 * GodotSurface.ets - XComponent wrapper for Godot Vulkan rendering
 *
 * Encapsulates an XComponent that serves as the rendering surface
 * for Godot's Vulkan renderer. Forwards keyboard, mouse, and touch
 * input events to the NAPI bridge, and manages the XComponent lifecycle.
 */
import { hilog } from '@kit.PerformanceAnalysisKit';
import { godot_napi } from 'libgodot_napi.so';

const TAG = 'GodotSurface';
const DOMAIN = 0x0000;

@ComponentV2
export struct GodotSurface {
  @Local surfaceId: string = '';
  private lastMouseX: number = 0;
  private lastMouseY: number = 0;

  aboutToAppear(): void {
    this.surfaceId = 'godot_surface_' + Date.now().toString();
    hilog.info(DOMAIN, TAG, '[INIT STEP 15/16] GodotSurface aboutToAppear, id: ' + this.surfaceId);
  }

  aboutToDisappear(): void {
    hilog.info(DOMAIN, TAG, 'Destroying surface: ' + this.surfaceId);
    try {
      godot_napi.onSurfaceDestroy();
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'NAPI surface destroy error: ' + e);
    }
  }

  // ---- Keyboard Event Forwarding ----
  //
  // OHOS key events arrive as onKeyEvent on the XComponent.
  // type: KeyType.Down=0, KeyType.Up=1
  // keyCode: OHOS key code (e.g. 2014=Escape, 2017=Up)
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
  // button: MouseButton.Left=0, Right=1, Middle=2
  // action: MouseAction.Press=1, Release=2, Move=3
  //
  // OHOS button order differs from the C++ side (which expects
  // 0=Left, 1=Middle, 2=Right). Convert OHOS Right(1)↔Middle(2)
  // before forwarding.
  handleMouseEvent(event: MouseEvent): void {
    const x: number = event.x ?? 0;
    const y: number = event.y ?? 0;
    const offsetX: number = x - this.lastMouseX;
    const offsetY: number = y - this.lastMouseY;
    this.lastMouseX = x;
    this.lastMouseY = y;

    // Map OHOS MouseButton → C++ expected order
    //   OHOS: Left=0, Right=1, Middle=2
    //   C++:  Left=0, Middle=1, Right=2
    let rawButton: number = event.button ?? 0;
    let button: number = rawButton;
    if (rawButton === 1) {
      button = 2; // Right → Middle
    } else if (rawButton === 2) {
      button = 1; // Middle → Right
    }

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

    try {
      godot_napi.sendMouseEvent(button, action, x, y, offsetX, offsetY);
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
        godot_napi.sendTouchEvent(t.id, action, t.x, t.y);
      } catch (e) {
        hilog.error(DOMAIN, TAG, 'Touch event error: ' + e);
      }
    }

    // Prevent default handling to avoid conflicts
    event.stopPropagation();
  }

  build() {
    Column() {
      XComponent({
        id: this.surfaceId,
        type: XComponentType.SURFACE,
        libraryname: 'godot_napi'
      })
        .width('100%')
        .height('100%')
        .backgroundColor(Color.Black)
        .focusable(true)
        .focusOnTouch(true)
        .onLoad((context?: object) => {
          hilog.info(DOMAIN, TAG, '[INIT STEP 16/16] XComponent onLoad: ' + this.surfaceId);
          try {
            if (context) {
              // Hand the native context to NAPI so it can resolve the
              // OH_NativeXComponent handle and register surface callbacks.
              godot_napi.initXComponent(context);
            }
            godot_napi.onSurfaceCreated(this.surfaceId);
          } catch (e) {
            hilog.error(DOMAIN, TAG, 'NAPI surface created error: ' + e);
          }
        })
        .onDestroy(() => {
          hilog.info(DOMAIN, TAG, 'XComponent onDestroy: ' + this.surfaceId);
          try {
            godot_napi.onSurfaceDestroy();
          } catch (e) {
            hilog.error(DOMAIN, TAG, 'NAPI surface destroy error: ' + e);
          }
        })
        .onKeyEvent((event: KeyEvent) => {
          this.handleKeyEvent(event);
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
