/**
 * TestAbility — 测试专用 UIAbility
 *
 * hypium 测试需要一个宿主 Ability 来加载被测页面。
 * 测试套件运行时，GodotEditorPage 会被 TestAbility 加载，
 * 测试用例通过 UiTest Driver 与页面交互。
 */
import { UIAbility } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { BusinessError } from '@kit.BasicServicesKit';
import { hilog } from '@kit.PerformanceAnalysisKit';

const TAG = 'TestAbility';
const DOMAIN = 0x0000;

export default class TestAbility extends UIAbility {
  onWindowStageCreate(windowStage: window.WindowStage): void {
    hilog.info(DOMAIN, TAG, 'TestAbility onWindowStageCreate');

    windowStage.loadContent('pages/GodotEditorPage', (err: BusinessError) => {
      if (err.code) {
        hilog.error(DOMAIN, TAG, `Failed to load test content: ${err.code}`);
        return;
      }
      hilog.info(DOMAIN, TAG, 'TestAbility: GodotEditorPage loaded');
    });

    windowStage.getMainWindow().then((mainWindow: window.Window) => {
      mainWindow.setWindowBackgroundColor('#1E1E2E');
      mainWindow.setWindowTitle('Godot Editor - Test Mode');
    }).catch((err: BusinessError) => {
      hilog.error(DOMAIN, TAG, `Failed to get main window: ${err.message}`);
    });
  }

  onWindowStageDestroy(): void {
    hilog.info(DOMAIN, TAG, 'TestAbility onWindowStageDestroy');
  }
}
