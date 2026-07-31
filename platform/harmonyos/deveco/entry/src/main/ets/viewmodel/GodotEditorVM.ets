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

  async initEngine(path: string): Promise<void> {
    this.projectPath = path;
    this.statusMessage = 'Loading project...';
    this.isLoading = true;

    hilog.info(DOMAIN, TAG, '[INIT STEP 4/16] initEngine called, project: ' + path);
    try {
      hilog.info(DOMAIN, TAG, '[INIT STEP 5/16] calling godot_napi.loadLibrary()');
      let loadResult: number = godot_napi.loadLibrary();
      hilog.info(DOMAIN, TAG, '[INIT STEP 5/16] loadLibrary returned: ' + loadResult);

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
