import { UIAbility } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { godot_napi } from 'libgodot_napi.so';

const TAG = 'GodotEditor';
const DOMAIN = 0x0000;

export default class EntryAbility extends UIAbility {
  private mainWindow?: window.Window;

  onWindowStageCreate(windowStage: window.WindowStage): void {
    hilog.info(DOMAIN, TAG, 'EntryAbility onWindowStageCreate');

    windowStage.loadContent('pages/GodotEditorPage', (err: BusinessError) => {
      if (err.code) {
        hilog.error(DOMAIN, TAG, `Failed to load content: ${err.code}`);
        return;
      }
      hilog.info(DOMAIN, TAG, 'GodotEditorPage loaded successfully');
    });

    // Configure window for PC: free-form window mode, dark theme
    windowStage.getMainWindow().then((mainWindow: window.Window) => {
      this.mainWindow = mainWindow;

      // Start maximized; user can restore to floating window
      mainWindow.maximize(window.MaximizePresentation.ENTER_IMMERSIVE);
      mainWindow.setWindowBackgroundColor('#1E1E2E');
      mainWindow.setWindowTitle('Godot Editor - HarmonyOS');

      // Listen for window size / mode changes so UI can adapt
      mainWindow.on('windowSizeChange', (size: window.Size) => {
        hilog.info(DOMAIN, TAG,
          `Window resized: ${size.width}x${size.height}`);
      });

      mainWindow.on('freeWindowModeChange', (mode: boolean) => {
        hilog.info(DOMAIN, TAG, `Window mode changed to: ${mode}`);
      });

      // Register NAPI callback for window title changes from engine
      try {
        godot_napi.registerTitleCallback((title: string) => {
          mainWindow.setWindowTitle(title);
        });
      } catch (e) {
        hilog.error(DOMAIN, TAG, `Failed to register title callback: ${e}`);
      }

      // Handle back button press
      mainWindow.on('keyboardHeightChange', () => {
        // placeholder for future IME handling
      });
    }).catch((err: BusinessError) => {
      hilog.error(DOMAIN, TAG, `Failed to get main window: ${err.code} ${err.message}`);
    });
  }

  onWindowStageDestroy(): void {
    hilog.info(DOMAIN, TAG, 'EntryAbility onWindowStageDestroy');
  }

  onForeground(): void {
    hilog.info(DOMAIN, TAG, 'EntryAbility onForeground');
    try {
      godot_napi.onResume();
    } catch (e) {
      hilog.error(DOMAIN, TAG, `NAPI onResume error: ${e}`);
    }
  }

  onBackground(): void {
    hilog.info(DOMAIN, TAG, 'EntryAbility onBackground');
    try {
      godot_napi.onPause();
    } catch (e) {
      hilog.error(DOMAIN, TAG, `NAPI onPause error: ${e}`);
    }
  }

  onBackPressed(): boolean {
    hilog.info(DOMAIN, TAG, 'EntryAbility onBackPressed');
    try {
      godot_napi.onBackPress();
    } catch (e) {
      hilog.error(DOMAIN, TAG, `NAPI onBackPress error: ${e}`);
    }
    return true;
  }
}
