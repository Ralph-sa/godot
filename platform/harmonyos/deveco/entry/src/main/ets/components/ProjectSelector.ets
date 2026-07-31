/**
 * ProjectSelector.ets - Native file browser for Godot project selection
 *
 * Provides a HarmonyOS-native UI for selecting a Godot project
 * before the engine is fully initialized. Uses system file picker
 * and directory browsing APIs.
 */
import { fileIo } from '@kit.CoreFileKit';
import { picker } from '@kit.CoreFileKit';
import { promptAction } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';

const TAG = 'ProjectSelector';

/** Callback when user selects a project directory */
export type ProjectSelectCallback = (projectPath: string) => void;

@ComponentV2
export struct ProjectSelector {
  @Param @Require onProjectSelected?: ProjectSelectCallback;
  @Local recentProjects: string[] = [];
  @Local showRecent: boolean = true;

  aboutToAppear(): void {
    this.loadRecentProjects();
  }

  /**
   * Load recently opened projects from preferences.
   */
  private loadRecentProjects(): void {
    try {
      let prefsPath: string = this.getPrefsPath();
      if (fileIo.accessSync(prefsPath)) {
        let content: string = fileIo.readTextSync(prefsPath);
        this.recentProjects = JSON.parse(content);
      }
    } catch (e) {
      hilog.warn(0x0000, TAG, `Failed to load recent projects: ${e}`);
    }
  }

  /**
   * Save recently opened projects to preferences.
   */
  private saveRecentProjects(): void {
    try {
      let prefsPath: string = this.getPrefsPath();
      let jsonStr: string = JSON.stringify(this.recentProjects.slice(0, 10));
      let fd = fileIo.openSync(prefsPath, fileIo.OpenMode.CREATE |
        fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
      fileIo.writeSync(fd.fd, jsonStr);
      fileIo.closeSync(fd);
    } catch (e) {
      hilog.warn(0x0000, TAG, `Failed to save recent projects: ${e}`);
    }
  }

  /**
   * Add a project path to recent list.
   */
  private addRecentProject(projectPath: string): void {
    let idx: number = this.recentProjects.indexOf(projectPath);
    if (idx >= 0) {
      this.recentProjects.splice(idx, 1);
    }
    this.recentProjects.unshift(projectPath);
    if (this.recentProjects.length > 10) {
      this.recentProjects = this.recentProjects.slice(0, 10);
    }
    this.saveRecentProjects();
  }

  private getPrefsPath(): string {
    let context = getContext(this);
    return context.filesDir + '/godot_recent_projects.json';
  }

  /**
   * Open the system directory picker and validate the selection.
   */
  private async browseForProject(): Promise<void> {
    try {
      let documentPicker = new picker.DocumentViewPicker();
      let options = new picker.DocumentSelectOptions();
      options.maxSelectNumber = 1;

      documentPicker.select(options).then((uris: string[]) => {
        if (uris && uris.length > 0) {
          let selectedUri = uris[0];
          hilog.info(0x0000, TAG, `Selected path: ${selectedUri}`);

          // Validate it's a Godot project (contains project.godot)
          this.validateAndOpen(selectedUri);
        }
      }).catch((err: Error) => {
        hilog.error(0x0000, TAG, `File picker error: ${err.message}`);
      });
    } catch (e) {
      hilog.error(0x0000, TAG, `Browse error: ${e}`);
    }
  }

  /**
   * Validate that the selected directory contains a Godot project
   * and invoke the callback.
   */
  private validateAndOpen(directoryPath: string): void {
    try {
      let projectFile: string = directoryPath + '/project.godot';

      if (fileIo.accessSync(projectFile)) {
        // Valid Godot project found
        this.addRecentProject(directoryPath);

        if (this.onProjectSelected) {
          this.onProjectSelected(directoryPath);
        }

        promptAction.showToast({
          message: 'Project opened: ' + directoryPath,
          duration: 2000
        });
      } else {
        // Not a valid Godot project
        promptAction.showDialog({
          title: 'Invalid Project',
          message: 'The selected directory does not contain a project.godot file.\n\nWould you like to create a new project here?',
          buttons: [
            { text: 'Cancel', color: '#999999' },
            { text: 'Create New', color: '#FF6B6B' }
          ]
        }).then((result) => {
          if (result.index === 1) {
            this.createNewProject(directoryPath);
          }
        });
      }
    } catch (e) {
      hilog.error(0x0000, TAG, `Validate error: ${e}`);
      promptAction.showToast({
        message: 'Failed to access directory',
        duration: 2000
      });
    }
  }

  /**
   * Create a minimal Godot project in the selected directory.
   */
  private createNewProject(directoryPath: string): void {
    try {
      // Create a minimal project.godot file
      let projectConfig: string = `; Engine configuration file.
; It's best edited using the editor UI and not directly,
; since the parameters that go here are not all obvious.
;
; Format:
;   [section] ; section goes between []
;   param=value ; assign values to parameters

config_version=5

[application]

config/name="New Godot Project"
`;

      let projectFile: string = directoryPath + '/project.godot';
      let fd = fileIo.openSync(projectFile, fileIo.OpenMode.CREATE |
        fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
      fileIo.writeSync(fd.fd, projectConfig);
      fileIo.closeSync(fd);

      this.addRecentProject(directoryPath);

      if (this.onProjectSelected) {
        this.onProjectSelected(directoryPath);
      }

      promptAction.showToast({
        message: 'New project created',
        duration: 2000
      });
    } catch (e) {
      hilog.error(0x0000, TAG, `Create project error: ${e}`);
      promptAction.showToast({
        message: 'Failed to create project',
        duration: 2000
      });
    }
  }

  /**
   * Open a recently used project.
   */
  private openRecentProject(projectPath: string): void {
    this.validateAndOpen(projectPath);
  }

  /**
   * Remove a project from recent list.
   */
  private removeRecentProject(index: number): void {
    this.recentProjects.splice(index, 1);
    this.saveRecentProjects();
  }

  build() {
    Column() {
      // Header
      Row() {
        Text('Godot Editor')
          .fontSize(28)
          .fontWeight(FontWeight.Bold)
          .fontColor('#FFFFFF')
      }
      .width('100%')
      .padding({ top: 32, bottom: 24 })
      .justifyContent(FlexAlign.Center)

      // Project Manager title
      Text('Project Manager')
        .fontSize(20)
        .fontColor('#88FFFFFF')
        .margin({ bottom: 32 })

      // Main actions
      Column() {
        // Browse button
        Button() {
          Row() {
            Image($r('app.media.app_icon'))
              .width(24)
              .height(24)
              .margin({ right: 12 })
            Text('Open Project')
              .fontSize(16)
              .fontWeight(FontWeight.Medium)
          }
        }
        .width(220)
        .height(48)
        .backgroundColor('#FF6B6B')
        .borderRadius(8)
        .onClick(() => {
          this.browseForProject();
        })

        // New Project button
        Button() {
          Row() {
            Image($r('app.media.app_icon'))
              .width(24)
              .height(24)
              .margin({ right: 12 })
            Text('New Project')
              .fontSize(16)
              .fontWeight(FontWeight.Medium)
          }
        }
        .width(220)
        .height(48)
        .backgroundColor('#3A3A4A')
        .borderRadius(8)
        .margin({ top: 12 })
        .onClick(() => {
          this.browseForProject();
        })
      }
      .margin({ bottom: 40 })

      // Recent projects section
      if (this.recentProjects.length > 0) {
        Column() {
          Divider()
            .color('#333344')
            .margin({ bottom: 20 })

          Text('Recent Projects')
            .fontSize(14)
            .fontColor('#666688')
            .margin({ bottom: 12 })

          List() {
            ForEach(this.recentProjects, (projectPath: string, index: number) => {
              ListItem() {
                Row() {
                  Text(projectPath)
                    .fontSize(14)
                    .fontColor('#CCCCCC')
                    .layoutWeight(1)
                    .maxLines(1)
                    .textOverflow({ overflow: TextOverflow.Ellipsis })

                  Button('X')
                    .width(28)
                    .height(28)
                    .fontSize(12)
                    .fontColor('#FF6B6B')
                    .backgroundColor('#00000000')
                    .onClick(() => {
                      this.removeRecentProject(index);
                    })
                }
                .width('100%')
                .padding({ left: 16, right: 8, top: 8, bottom: 8 })
              }
              .onClick(() => {
                this.openRecentProject(projectPath);
              })
              .backgroundColor('#252535')
              .borderRadius(6)
              .margin({ bottom: 4 })
            }, (item: string, index: number) => index.toString())
          }
          .width('100%')
          .constraintSize({ maxHeight: 200 })
        }
        .width('100%')
        .constraintSize({ maxWidth: 500 })
        .padding({ left: 16, right: 16 })
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#1E1E2E')
    .justifyContent(FlexAlign.Center)
  }
}
