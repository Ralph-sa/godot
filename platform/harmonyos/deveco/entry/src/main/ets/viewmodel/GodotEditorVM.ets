/**
 * GodotEditorVM.ets - MVVM ViewModel for Godot Editor
 *
 * Manages engine lifecycle, application state, and NAPI calls.
 * Architecture: the engine receives the project path at init time.
 */
import { hilog } from '@kit.PerformanceAnalysisKit';
import { godot_napi } from 'libgodot_napi.so';

const TAG = 'GodotEditorVM';
const DOMAIN = 0x0000;

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

    hilog.info(DOMAIN, TAG, '[INIT STEP 4/16] initEngine called, project: ' + path);
    try {
      hilog.info(DOMAIN, TAG, '[INIT STEP 5/16] calling godot_napi.init() — synchronous, blocks main thread');
      let result: number = godot_napi.init();
      hilog.info(DOMAIN, TAG, '[INIT STEP 15/16] godot_napi.init returned: ' + result);
      if (result === 0) {
        this.isEngineReady = true;
        this.isLoading = false;
        this.statusMessage = 'Engine ready';
        hilog.info(DOMAIN, TAG, '[INIT STEP 15/16] Engine OK — switching to GodotSurface');
      } else {
        this.isLoading = false;
        this.statusMessage = 'Engine init failed: ' + result;
        hilog.error(DOMAIN, TAG, '[INIT STEP 15/16] Engine FAILED: ' + result);
      }
    } catch (e) {
      this.isLoading = false;
      this.statusMessage = 'Engine init error';
      hilog.error(DOMAIN, TAG, '[INIT STEP 5/16] NAPI init EXCEPTION: ' + e);
    }
  }

  cleanupEngine(): void {
    hilog.info(DOMAIN, TAG, 'Cleaning up Godot engine...');
    try {
      godot_napi.cleanup();
      this.isEngineReady = false;
      this.surfaceCreated = false;
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'NAPI cleanup error: ' + e);
    }
  }

  onAppPause(): void {
    hilog.info(DOMAIN, TAG, 'App paused');
    try {
      godot_napi.onPause();
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'NAPI pause error: ' + e);
    }
  }

  onAppResume(): void {
    hilog.info(DOMAIN, TAG, 'App resumed');
    try {
      godot_napi.onResume();
    } catch (e) {
      hilog.error(DOMAIN, TAG, 'NAPI resume error: ' + e);
    }
  }
}
