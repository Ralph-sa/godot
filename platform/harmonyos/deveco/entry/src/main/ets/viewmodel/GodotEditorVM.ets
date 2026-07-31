/**
 * GodotEditorVM.ets - MVVM ViewModel for Godot Editor
 *
 * Manages engine lifecycle, application state, and NAPI calls.
 * Architecture: the engine receives the project path at init time.
 */
import { godot_napi } from 'libgodot_napi.so';

@ObservedV2
export class GodotEditorVM {
  @Trace isEngineReady: boolean = false;
  @Trace isLoading: boolean = true;
  @Trace statusMessage: string = 'Initializing engine...';
  @Trace surfaceCreated: boolean = false;
  @Trace projectPath: string = '';

  /**
   * Initialize the Godot engine with a project path.
   * @param path Absolute path to the Godot project directory.
   */
  initEngine(path: string): void {
    this.projectPath = path;
    this.statusMessage = 'Loading project...';
    this.isLoading = true;

    console.info(`[GodotEditorVM] Initializing Godot engine for project: ${path}`);
    try {
      // Store project path for engine to read during initialization
      // The engine reads this via command-line or environment variable

      let result: number = godot_napi.init();
      if (result === 0) {
        this.isEngineReady = true;
        this.isLoading = false;
        this.statusMessage = 'Engine ready';
        console.info('[GodotEditorVM] Engine initialized successfully');
      } else {
        this.isLoading = false;
        this.statusMessage = `Engine init failed: ${result}`;
        console.error('[GodotEditorVM] Engine init failed:', result);
      }
    } catch (e) {
      this.isLoading = false;
      this.statusMessage = 'Engine init error';
      console.error('[GodotEditorVM] NAPI init error:', e);
    }
  }

  cleanupEngine(): void {
    console.info('[GodotEditorVM] Cleaning up Godot engine...');
    try {
      godot_napi.cleanup();
      this.isEngineReady = false;
      this.surfaceCreated = false;
    } catch (e) {
      console.error('[GodotEditorVM] NAPI cleanup error:', e);
    }
  }

  onAppPause(): void {
    console.info('[GodotEditorVM] App paused');
    try {
      godot_napi.onPause();
    } catch (e) {
      console.error('[GodotEditorVM] NAPI pause error:', e);
    }
  }

  onAppResume(): void {
    console.info('[GodotEditorVM] App resumed');
    try {
      godot_napi.onResume();
    } catch (e) {
      console.error('[GodotEditorVM] NAPI resume error:', e);
    }
  }
}
