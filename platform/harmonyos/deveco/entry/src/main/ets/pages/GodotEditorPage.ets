import { hilog } from '@kit.PerformanceAnalysisKit';
import { GodotSurface } from '../components/GodotSurface';
import { ProjectSelector } from '../components/ProjectSelector';
import { GodotEditorVM } from '../viewmodel/GodotEditorVM';

const TAG = 'GodotEditorPage';
const DOMAIN = 0x0000;

/** 设为 true 则自动进入测试模式（跳过 ProjectSelector）。Debug 构建需要时启用。 */
const AUTO_TEST_MODE: boolean = false;

/**
 * Main Godot Editor page.
 *
 * Flow:
 * 1. Show ProjectSelector - user selects or creates a project.
 * 2. Initialize Godot engine with the selected project path.
 * 3. Display GodotSurface (Vulkan rendering surface).
 *
 * AUTO_TEST_MODE=true → 跳过 UI，直接初始化引擎用沙箱自动测试项目。
 */
@Entry
@ComponentV2
struct GodotEditorPage {
  @Local viewModel: GodotEditorVM = new GodotEditorVM();
  @Local projectSelected: boolean = false;
  @Local projectPath: string = '';

  aboutToAppear(): void {
    hilog.info(DOMAIN, TAG, 'GodotEditorPage aboutToAppear');

    if (AUTO_TEST_MODE) {
      const testProject: string = 'auto_test_project';
      hilog.info(DOMAIN, TAG, `Auto-test mode: using project "${testProject}"`);
      // 延迟一帧以等待 UI 就绪
      setTimeout(() => {
        this.onProjectSelected(testProject);
      }, 200);
    }
  }

  aboutToDisappear(): void {
    hilog.info(DOMAIN, TAG, 'GodotEditorPage aboutToDisappear');
    this.viewModel.cleanupEngine();
  }

  /**
   * Called when user selects a project from the ProjectSelector.
   */
  onProjectSelected = (path: string): void => {
    hilog.info(DOMAIN, TAG, `Project selected: ${path}`);
    this.projectPath = path;
    this.projectSelected = true;
    this.viewModel.initEngine(path);
  };

  build() {
    Column() {
      if (!this.projectSelected) {
        // Phase 1: Project selection
        ProjectSelector({ onProjectSelected: this.onProjectSelected })
      } else if (!this.viewModel.isEngineReady) {
        // Phase 2: Loading
        Column() {
          LoadingProgress()
            .width(60)
            .height(60)
            .color('#FF6B6B')
          Text(this.viewModel.statusMessage)
            .fontSize(16)
            .fontColor('#CCCCCC')
            .margin({ top: 16 })
        }
        .width('100%')
        .height('100%')
        .justifyContent(FlexAlign.Center)
        .alignItems(HorizontalAlign.Center)
      } else {
        // Phase 3: Godot rendering surface
        GodotSurface()
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#1E1E2E')
  }
}
