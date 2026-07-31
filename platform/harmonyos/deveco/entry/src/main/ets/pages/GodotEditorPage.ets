import { hilog } from '@kit.PerformanceAnalysisKit';
import { GodotSurface } from '../components/GodotSurface';
import { ProjectSelector } from '../components/ProjectSelector';
import { GodotEditorVM } from '../viewmodel/GodotEditorVM';

const TAG = 'GodotEditorPage';

/**
 * Main Godot Editor page.
 *
 * Flow:
 * 1. Show ProjectSelector - user selects or creates a project.
 * 2. Initialize Godot engine with the selected project path.
 * 3. Display GodotSurface (Vulkan rendering surface).
 */
@Entry
@ComponentV2
struct GodotEditorPage {
  @Local viewModel: GodotEditorVM = new GodotEditorVM();
  @Local projectSelected: boolean = false;
  @Local projectPath: string = '';

  aboutToAppear(): void {
    hilog.info(0x0000, TAG, 'GodotEditorPage aboutToAppear');
  }

  aboutToDisappear(): void {
    hilog.info(0x0000, TAG, 'GodotEditorPage aboutToDisappear');
    this.viewModel.cleanupEngine();
  }

  /**
   * Called when user selects a project from the ProjectSelector.
   */
  onProjectSelected = (path: string): void => {
    hilog.info(0x0000, TAG, `Project selected: ${path}`);
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
