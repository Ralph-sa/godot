import { common } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { GodotSurface } from '../components/GodotSurface';
import { ProjectSelector } from '../components/ProjectSelector';
import { GodotEditorVM } from '../viewmodel/GodotEditorVM';
import { AutoTestMode } from '../model/AutoTestMode';

const TAG = 'GodotEditorPage';
const DOMAIN = 0x0000;

/**
 * Main Godot Editor page.
 *
 * Flow:
 * 1. Show ProjectSelector - user selects or creates a project.
 * 2. Initialize Godot engine with the selected project path.
 * 3. Display GodotSurface (Vulkan rendering surface).
 *
 * Step 1 is skipped when the app was launched in auto-test mode, since an
 * automated run has no way to answer the selector prompt.
 */
@Entry
@ComponentV2
struct GodotEditorPage {
  @Local viewModel: GodotEditorVM = new GodotEditorVM();
  @Local projectSelected: boolean = false;
  @Local projectPath: string = '';

  aboutToAppear(): void {
    hilog.info(DOMAIN, TAG, 'GodotEditorPage aboutToAppear');
    if (AutoTestMode.isEnabled()) {
      this.startAutoTestRun();
    }
  }

  /**
   * Boots straight into a sandbox project so a smoke test can reach the engine
   * without UI interaction.
   */
  private startAutoTestRun(): void {
    let ctx = this.getUIContext().getHostContext() as common.UIAbilityContext;
    let path: string = AutoTestMode.ensureTestProject(ctx.filesDir);
    if (path.length === 0) {
      hilog.error(DOMAIN, TAG, 'AUTOTEST: failed to prepare project, falling back to selector');
      return;
    }
    hilog.info(DOMAIN, TAG, `AUTOTEST: bypassing selector, project: ${path}`);
    this.onProjectSelected(path);
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
