/**
 * GodotEditorVM.ets - MVVM ViewModel for Godot Editor
 *
 * All heavy init (dlopen + Main::setup) runs on pthread worker.
 * Main thread polls via await setTimeout (avoids ANR).
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
  private pendingSurfaceId: string = '';
  private pendingSurfaceWidth: number = 0;
  private pendingSurfaceHeight: number = 0;
  private pendingSurfaceGeneration: number = 0;

  async initEngine(path: string, filesDir: string, cacheDir: string, tempDir: string,
    surfaceId: string, width: number, height: number, generation: number): Promise<void> {
    this.projectPath = path;
    this.statusMessage = 'Loading project...';
    this.isLoading = true;

    if (path.length === 0 || filesDir.length === 0 || cacheDir.length === 0 || tempDir.length === 0 ||
      surfaceId.length === 0 || width <= 0 || height <= 0 || generation <= 0) {
      this.isLoading = false;
      this.statusMessage = 'Invalid project or surface';
      hilog.error(DOMAIN, TAG, `Invalid engine init arguments: path=${path} files=${filesDir} surface=${surfaceId} ${width}x${height}`);
      return;
    }

    hilog.info(DOMAIN, TAG, `[INIT STEP 5/17] initEngine project=${path} surface=${surfaceId} ${width}x${height}`);
    try {
      hilog.info(DOMAIN, TAG, '[INIT STEP 6/17] calling godot_napi.loadLibrary with sandbox roots and surface');
      let loadResult: number = godot_napi.loadLibrary(path, filesDir, cacheDir, tempDir,
        surfaceId, width, height, generation);
      hilog.info(DOMAIN, TAG, '[INIT STEP 6/17] loadLibrary returned: ' + loadResult);

      if (loadResult !== 0) {
        this.isLoading = false;
        this.statusMessage = 'Failed to start library load';
        hilog.error(DOMAIN, TAG, '[INIT STEP 5/16] loadLibrary FAILED');
        return;
      }

      // Poll with await setTimeout — this works in ArkTS
      let pollCount: number = 0;
      let pollStatus: number = 0;
      while (pollStatus === 0) {
        await new Promise<void>((resolve: Function) => { setTimeout(resolve, 2000); });
        pollStatus = godot_napi.isLibraryLoaded();
        pollCount++;
        hilog.info(DOMAIN, TAG, '[INIT STEP 7-9/16] Poll #' + pollCount + ' status=' + pollStatus);
      }

      if (pollStatus < 0) {
        this.isLoading = false;
        this.statusMessage = 'Engine init failed';
        hilog.error(DOMAIN, TAG, '[INIT STEP 7-9/16] Engine init FAILED');
        return;
      }

      hilog.info(DOMAIN, TAG, '[INIT STEP 9/16] Init complete after ' + pollCount + ' polls');

      let result: number = godot_napi.startEngine();
      hilog.info(DOMAIN, TAG, '[INIT STEP 15/16] startEngine returned: ' + result);

      if (result === 0) {
        this.isEngineReady = true;
        this.isLoading = false;
        this.statusMessage = 'Engine ready';
        hilog.info(DOMAIN, TAG, '[INIT STEP 15/16] Engine OK — switching to GodotSurface');
        if (this.pendingSurfaceGeneration > 0) {
          this.forwardSurfaceCreated(this.pendingSurfaceId, this.pendingSurfaceWidth,
            this.pendingSurfaceHeight, this.pendingSurfaceGeneration);
          this.pendingSurfaceId = '';
          this.pendingSurfaceWidth = 0;
          this.pendingSurfaceHeight = 0;
          this.pendingSurfaceGeneration = 0;
        }
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

  private forwardSurfaceCreated(surfaceId: string, width: number, height: number,
    generation: number): void {
    try {
      let result: number = godot_napi.onSurfaceCreated(surfaceId, width, height, generation);
      this.surfaceCreated = result === 0;
      hilog.info(DOMAIN, TAG,
        `Surface forwarded: ${surfaceId} generation=${generation} ${width}x${height}, result=${result}`);
    } catch (e) {
      this.surfaceCreated = false;
      hilog.error(DOMAIN, TAG, 'NAPI recreated surface error: ' + e);
    }
  }

  onSurfaceCreated(surfaceId: string, width: number, height: number, generation: number): void {
    if (surfaceId.length === 0 || width <= 0 || height <= 0 || generation <= 0) {
      hilog.error(DOMAIN, TAG, 'Ignoring invalid recreated surface');
      return;
    }

    if (!this.isEngineReady) {
      this.pendingSurfaceId = surfaceId;
      this.pendingSurfaceWidth = width;
      this.pendingSurfaceHeight = height;
      this.pendingSurfaceGeneration = generation;
      hilog.info(DOMAIN, TAG,
        `Surface update deferred until engine ready: ${surfaceId} generation=${generation} ${width}x${height}`);
      return;
    }

    this.forwardSurfaceCreated(surfaceId, width, height, generation);
  }

  cleanupEngine(): void {
    hilog.info(DOMAIN, TAG, 'Cleaning up Godot engine...');
    try {
      let cleanupResult: number = godot_napi.cleanup();
      if (cleanupResult === 0) {
        this.isEngineReady = false;
        this.surfaceCreated = false;
      } else {
        hilog.error(DOMAIN, TAG, 'NAPI cleanup request rejected: ' + cleanupResult);
      }
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
