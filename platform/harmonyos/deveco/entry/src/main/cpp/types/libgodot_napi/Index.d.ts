/**
 * NAPI TypeScript declarations for Godot Engine bridge
 */
export const godot_napi: {
  /**
   * Initialize the Godot engine asynchronously.
   * dlopen of libgodot.so (151MB) runs on a worker thread to prevent ANR.
   * The callback is fired on the main thread once loading is complete.
   * @param callback (result: number) => void — 0 = success, negative = error
   */
  init: (callback: (result: number) => void) => void;

  /** Cleanup the Godot engine. Returns 0 on success. */
  cleanup: () => number;

  /** Notify engine that rendering surface was created. */
  onSurfaceCreated: (surfaceId: string) => number;

  /** Notify engine that rendering surface was destroyed. */
  onSurfaceDestroy: () => number;

  /** Send a keyboard event to the engine.
   * @param keyCode OHOS key code
   * @param eventType 0=down, 1=up
   * @param keyText Text representation of the key
   */
  sendKeyEvent: (keyCode: number, eventType: number, keyText: string) => number;

  /** Send a mouse event to the engine.
   * @param button Mouse button index
   * @param action 0=press, 1=release, 2=move
   * @param x X position in surface coordinates
   * @param y Y position in surface coordinates
   * @param offsetX Scroll offset X
   * @param offsetY Scroll offset Y
   */
  sendMouseEvent: (button: number, action: number, x: number, y: number,
    offsetX: number, offsetY: number) => number;

  /** Send a touch event to the engine.
   * @param touchId Touch point index
   * @param action 0=down, 1=up, 2=move
   * @param x X position in surface coordinates
   * @param y Y position in surface coordinates
   */
  sendTouchEvent: (touchId: number, action: number, x: number, y: number) => number;

  /** Send text input (IME) to the engine.
  * @param text The input text string
  */
  sendInputText: (text: string) => number;

  /** Notify engine that app is going to background (pause). */
  onPause: () => number;

  /** Notify engine that app is returning to foreground (resume). */
  onResume: () => number;

  /** Notify engine of back button press. */
  onBackPress: () => number;

  /** Register a callback for window title changes (C++ -> ArkTS).
   * @param callback Function(title: string) => void
   */
  registerTitleCallback: (callback: (title: string) => void) => number;
};
