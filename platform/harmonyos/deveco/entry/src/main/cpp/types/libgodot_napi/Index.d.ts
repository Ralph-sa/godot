/**
 * NAPI TypeScript declarations for Godot Engine bridge
 */
export const godot_napi: {
  /** Atomically rename one staging project directory under filesDir/projects.
   * Both names are validated native leaf names. The destination is never replaced.
   */
  commitProjectImport: (filesDir: string, stagingLeaf: string, destinationLeaf: string) => number;

  /** Recursively remove one staging project directory without following symlinks. */
  cleanupProjectImport: (filesDir: string, stagingLeaf: string) => number;

  /** Start asynchronous loading after a valid XComponent surface exists.
   * The project path must be an app-sandbox filesystem path containing
   * project.godot. Sandbox roots come from UIAbilityContext; surface
   * dimensions are physical pixels.
   */
  loadLibrary: (projectPath: string, filesDir: string, cacheDir: string, tempDir: string,
    surfaceId: string, width: number, height: number, generation: number) => number;

  /** Poll asynchronous setup. Returns 1=ready, 0=loading, -1=failed. */
  isLibraryLoaded: () => number;

  /** Return the setup result after isLibraryLoaded reports ready. */
  startEngine: () => number;

  /** Cleanup the Godot engine. Returns 0 on success. */
  cleanup: () => number;

  /** Notify engine that rendering surface was created. */
  onSurfaceCreated: (surfaceId: string, width: number, height: number, generation: number) => number;

  /** Capture the XComponent.onLoad context and update engine input registration.
   * Must succeed before the surface is reported ready.
   */
  initXComponent: (context: object) => number;

  /** Notify engine that rendering surface was destroyed. */
  onSurfaceDestroy: (surfaceId: string, generation: number) => number;

  /** Send a keyboard event to the engine.
   * @param keyCode OHOS key code
   * @param eventType 0=down, 1=up
   * @param keyText Text representation of the key
   */
  sendKeyEvent: (keyCode: number, eventType: number, keyText: string) => number;

  /** Send a mouse event to the engine.
   * @param button Mouse button index
   * @param action 0=press, 1=release, 2=move
   * @param x X position in physical pixels
   * @param y Y position in physical pixels
   * @param offsetX Relative motion in physical pixels
   * @param offsetY Relative motion in physical pixels
   */
  sendMouseEvent: (button: number, action: number, x: number, y: number,
    offsetX: number, offsetY: number) => number;

  /** Send a touch event to the engine.
   * @param touchId Touch point index
   * @param action 0=down, 1=up, 2=move
   * @param x X position in physical pixels
   * @param y Y position in physical pixels
   */
  sendTouchEvent: (touchId: number, action: number, x: number, y: number) => number;

  /** Send text input (IME) to the engine.
  * @param text The input text string
  */
  sendInputText: (text: string) => number;

  /** Update IME preview/composition text and selection. */
  sendImeUpdate: (text: string, selectionStart: number, selectionLength: number) => number;

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
