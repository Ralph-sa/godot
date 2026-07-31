import { UIAbility } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { godot_napi } from 'libgodot_napi.so';

const TAG = 'GodotEditor';

export default class EntryAbility extends UIAbility {
  onWindowStageCreate(windowStage: window.WindowStage): void {
    hilog.info(0x0000, TAG, 'EntryAbility onWindowStageCreate');

    windowStage.loadContent('pages/GodotEditorPage', (err: BusinessError) => {
      if (err.code) {
        hilog.error(0x0000, TAG, `Failed to load content: ${err.code}`);
        return;
      }
      hilog.info(0x0000, TAG, 'GodotEditorPage loaded successfully');
    });

    // Configure window for PC: free-form window mode, dark theme
    windowStage.getMainWindow().then((mainWindow: window.Window) => {
      // Maximize for best editor experience
      mainWindow.maximize(window.MaximizePresentation.ENTER_IMMERSIVE);
      mainWindow.setWindowBackgroundColor('#1E1E2E');

      // Set window title
      mainWindow.setWindowTitle('Godot Editor - HarmonyOS');

      // Register NAPI callback for window title changes from engine
      try {
        godot_napi.registerTitleCallback((title: string) => {
          mainWindow.setWindowTitle(title);
        });
      } catch (e) {
        hilog.error(0x0000, TAG, `Failed to register title callback: ${e}`);
      }

      // Handle back button press
      mainWindow.on('keyboardHeightChange', () => {
        // placeholder for future IME handling
      });
    });
  }

  onWindowStageDestroy(): void {
    hilog.info(0x0000, TAG, 'EntryAbility onWindowStageDestroy');
  }

  onForeground(): void {
    hilog.info(0x0000, TAG, 'EntryAbility onForeground');
    try {
      godot_napi.onResume();
    } catch (e) {
      hilog.error(0x0000, TAG, `NAPI onResume error: ${e}`);
    }
  }

  onBackground(): void {
    hilog.info(0x0000, TAG, 'EntryAbility onBackground');
    try {
      godot_napi.onPause();
    } catch (e) {
      hilog.error(0x0000, TAG, `NAPI onPause error: ${e}`);
    }
  }

  onBackPressed(): boolean {
    hilog.info(0x0000, TAG, 'EntryAbility onBackPressed');
    try {
      godot_napi.onBackPress();
    } catch (e) {
      hilog.error(0x0000, TAG, `NAPI onBackPress error: ${e}`);
    }
    return true;
  }
}
