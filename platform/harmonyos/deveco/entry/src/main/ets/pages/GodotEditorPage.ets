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
 * 2. Create GodotSurface and wait for a real XComponent surface id.
 * 3. Initialize Godot with the selected path and native surface metadata.
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
  @Local engineInitRequested: boolean = false;

  private getCtx(): common.UIAbilityContext {
    return this.getUIContext().getHostContext() as common.UIAbilityContext;
  }

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
    let ctx: common.UIAbilityContext = this.getCtx();
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
    this.engineInitRequested = false;
    this.projectSelected = true;
  };

  /**
   * The Windows port owns a real main window before Main::setup constructs the
   * DisplayServer. HarmonyOS must provide the equivalent XComponent surface
   * first; starting the engine before this callback leaves RenderingDevice
   * without a swap chain and crashes during editor setup.
   */
  onSurfaceReady = (surfaceId: string, width: number, height: number, generation: number): void => {
    if (!this.projectSelected || this.projectPath.length === 0) {
      hilog.error(DOMAIN, TAG, 'Surface became ready without a selected project');
      return;
    }

    if (this.engineInitRequested) {
      // A later XComponent recreation belongs to the already-running engine.
      // Forward it even while Main::setup is still running: the native bridge
      // queues surface operations and the engine thread applies them in order.
      this.viewModel.onSurfaceCreated(surfaceId, width, height, generation);
      return;
    }

    this.engineInitRequested = true;
    let ctx: common.UIAbilityContext = this.getCtx();
    hilog.info(DOMAIN, TAG, `Surface ready before engine init: ${surfaceId} ${width}x${height}`);
    this.viewModel.initEngine(this.projectPath, ctx.filesDir, ctx.cacheDir, ctx.tempDir,
      surfaceId, width, height, generation);
  };

  build() {
    Stack({ alignContent: Alignment.Center }) {
      if (!this.projectSelected) {
        // Phase 1: Project selection
        ProjectSelector({ onProjectSelected: this.onProjectSelected })
      } else {
        // Phase 2: construct the rendering surface before engine setup.
        GodotSurface({ onSurfaceReady: this.onSurfaceReady })

        if (!this.viewModel.isEngineReady) {
          // Phase 3: keep the XComponent alive under a loading overlay while
          // Main::setup performs the heavy editor initialization.
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
          .backgroundColor('#E61E1E2E')
        }
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#1E1E2E')
  }
}
