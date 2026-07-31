/**
 * ProjectSelector.ets - Native project manager for Godot HarmonyOS
 *
 * "Open Project" uses system file picker to locate existing projects.
 * "New Project" creates a project directly in the app sandbox.
 */

import { fileIo } from '@kit.CoreFileKit';
import { picker } from '@kit.CoreFileKit';
import { promptAction } from '@kit.ArkUI';
import { hilog } from '@kit.PerformanceAnalysisKit';

const TAG = 'ProjectSelector';
const DOMAIN = 0x0000;

export type ProjectSelectCallback = (projectPath: string) => void;

@ComponentV2
export struct ProjectSelector {
  @Param @Require onProjectSelected?: ProjectSelectCallback;
  @Local recentProjects: string[] = [];
  @Local showNewProjectDialog: boolean = false;
  @Local newProjectName: string = '';

  aboutToAppear(): void {
    hilog.info(DOMAIN, TAG, 'ProjectSelector aboutToAppear');
    this.loadRecentProjects();
  }

  /** ---- Recent Projects Persistence ---- */

  private loadRecentProjects(): void {
    try {
      let prefsPath: string = this.getPrefsPath();
      hilog.info(DOMAIN, TAG, `Loading recent projects from: ${prefsPath}`);
      if (fileIo.accessSync(prefsPath)) {
        let content: string = fileIo.readTextSync(prefsPath);
        this.recentProjects = JSON.parse(content);
        hilog.info(DOMAIN, TAG, `Loaded ${this.recentProjects.length} recent projects`);
      } else {
        hilog.info(DOMAIN, TAG, 'No recent projects file found');
      }
    } catch (e) {
      hilog.warn(DOMAIN, TAG, `Failed to load recent projects: ${e}`);
    }
  }

  private saveRecentProjects(): void {
    try {
      let prefsPath: string = this.getPrefsPath();
      let jsonStr: string = JSON.stringify(this.recentProjects.slice(0, 10));
      let fd = fileIo.openSync(prefsPath, fileIo.OpenMode.CREATE |
        fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
      fileIo.writeSync(fd.fd, jsonStr);
      fileIo.closeSync(fd);
      hilog.info(DOMAIN, TAG, 'Recent projects saved');
    } catch (e) {
      hilog.warn(DOMAIN, TAG, `Failed to save recent projects: ${e}`);
    }
  }

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

  /** ---- Sandbox Project Creation ---- */

  /**
   * Get the sandbox root used for creating new projects.
   * Uses the app's filesDir as the base (maps to /data/storage/.../files).
   */
  private getSandboxProjectsDir(): string {
    let context = getContext(this);
    let dir = context.filesDir + '/projects';
    hilog.info(DOMAIN, TAG, `Sandbox projects dir: ${dir}`);
    return dir;
  }

  /**
   * Create a new Godot project in the app sandbox.
   */
  private createProjectInSandbox(projectName: string): string {
    let projectsDir = this.getSandboxProjectsDir();
    let projectDir = projectsDir + '/' + projectName;

    hilog.info(DOMAIN, TAG, `Creating project "${projectName}" at ${projectDir}`);

    // Ensure projects directory exists
    if (!fileIo.accessSync(projectsDir)) {
      hilog.info(DOMAIN, TAG, `Creating projects directory: ${projectsDir}`);
      fileIo.mkdirSync(projectsDir, true);
    }

    // Check if project already exists
    if (fileIo.accessSync(projectDir)) {
      hilog.warn(DOMAIN, TAG, `Project directory already exists: ${projectDir}`);
      promptAction.showToast({
        message: 'Project already exists!',
        duration: 2000
      });
      return '';
    }

    // Create project directory
    fileIo.mkdirSync(projectDir, true);
    hilog.info(DOMAIN, TAG, `Created project directory: ${projectDir}`);

    // Write project.godot
    let projectConfig: string =
      '; Engine configuration file.\n' +
      '; It\'s best edited using the editor UI and not directly,\n' +
      '; since the parameters that go here are not all obvious.\n' +
      ';\n' +
      '; Format:\n' +
      ';   [section] ; section goes between []\n' +
      ';   param=value ; assign values to parameters\n' +
      '\n' +
      'config_version=5\n' +
      '\n' +
      '[application]\n' +
      '\n' +
      'config/name="' + projectName + '"\n';

    let projectFile = projectDir + '/project.godot';
    let fd = fileIo.openSync(projectFile, fileIo.OpenMode.CREATE |
      fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
    fileIo.writeSync(fd.fd, projectConfig);
    fileIo.closeSync(fd);

    hilog.info(DOMAIN, TAG, `project.godot written to: ${projectFile}`);
    return projectDir;
  }

  /** ---- New Project Dialog ---- */

  private showNewProjectInput(): void {
    hilog.info(DOMAIN, TAG, 'Showing new project dialog');
    this.newProjectName = '';
    this.showNewProjectDialog = true;
  }

  private confirmNewProject(): void {
    hilog.info(DOMAIN, TAG, `Confirm new project: "${this.newProjectName}"`);
    this.showNewProjectDialog = false;

    let name = this.newProjectName.trim();
    if (name.length === 0) {
      hilog.warn(DOMAIN, TAG, 'Project name is empty');
      promptAction.showToast({ message: 'Please enter a project name', duration: 2000 });
      return;
    }

    let projectPath = this.createProjectInSandbox(name);
    if (projectPath.length > 0) {
      this.addRecentProject(projectPath);
      if (this.onProjectSelected) {
        hilog.info(DOMAIN, TAG, `Firing onProjectSelected: ${projectPath}`);
        this.onProjectSelected(projectPath);
      }
      promptAction.showToast({ message: 'Project created: ' + name, duration: 2000 });
    }
  }

  private cancelNewProject(): void {
    hilog.info(DOMAIN, TAG, 'New project cancelled');
    this.showNewProjectDialog = false;
    this.newProjectName = '';
  }

  /** ---- Open Existing Project ---- */

  private async browseForProject(): Promise<void> {
    hilog.info(DOMAIN, TAG, 'Opening file picker');
    try {
      let documentPicker = new picker.DocumentViewPicker();
      let options = new picker.DocumentSelectOptions();
      options.maxSelectNumber = 1;

      documentPicker.select(options).then((uris: string[]) => {
        if (uris && uris.length > 0) {
          let selectedUri = uris[0];
          hilog.info(DOMAIN, TAG, `File picker returned: ${selectedUri}`);
          this.validateAndOpen(selectedUri);
        } else {
          hilog.info(DOMAIN, TAG, 'File picker cancelled (no selection)');
        }
      }).catch((err: Error) => {
        hilog.error(DOMAIN, TAG, `File picker error: ${err.message}`);
      });
    } catch (e) {
      hilog.error(DOMAIN, TAG, `Browse error: ${e}`);
    }
  }

  /**
   * Validate a directory as a Godot project.
   * If it's outside the sandbox but the user wants to create a project
   * there, redirect it into the sandbox.
   */
  private validateAndOpen(directoryPath: string): void {
    hilog.info(DOMAIN, TAG, `Validating: ${directoryPath}`);
    try {
      let projectFile: string = directoryPath + '/project.godot';

      if (fileIo.accessSync(projectFile)) {
        hilog.info(DOMAIN, TAG, `Valid Godot project found: ${directoryPath}`);
        this.addRecentProject(directoryPath);
        if (this.onProjectSelected) {
          this.onProjectSelected(directoryPath);
        }
        promptAction.showToast({ message: 'Project opened', duration: 2000 });
      } else {
        hilog.info(DOMAIN, TAG, `No project.godot in ${directoryPath}, offering to create`);
        // Extract project name from the path for the dialog message
        let parts = directoryPath.split('/');
        let folderName = parts[parts.length - 1] || 'NewProject';

        promptAction.showDialog({
          title: 'Invalid Project',
          message: 'The selected directory does not contain a project.godot file.\n\nCreate a new project here?',
          buttons: [
            { text: 'Cancel', color: '#999999' },
            { text: 'Create New', color: '#FF6B6B' }
          ]
        }).then((result) => {
          if (result.index === 1) {
            hilog.info(DOMAIN, TAG, `User chose to create project at: ${directoryPath}`);
            this.createAndOpenAt(directoryPath, folderName);
          } else {
            hilog.info(DOMAIN, TAG, 'User cancelled project creation');
          }
        });
      }
    } catch (e) {
      hilog.error(DOMAIN, TAG, `Validate error: ${e}`);
      promptAction.showToast({ message: 'Failed to access directory', duration: 2000 });
    }
  }

  /**
   * Create a project at a user-selected path.
   * If the path is outside the sandbox, fallback to sandbox with same project name.
   */
  private createAndOpenAt(directoryPath: string, fallbackName: string): void {
    hilog.info(DOMAIN, TAG, `createAndOpenAt: path=${directoryPath}, fallback=${fallbackName}`);
    try {
      // Try writing directly
      let projectFile = directoryPath + '/project.godot';
      let written = false;
      try {
        let fd = fileIo.openSync(projectFile, fileIo.OpenMode.CREATE |
          fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
        let projectConfig: string =
          '; Engine configuration file.\n' +
          'config_version=5\n\n' +
          '[application]\n\n' +
          'config/name="' + fallbackName + '"\n';
        fileIo.writeSync(fd.fd, projectConfig);
        fileIo.closeSync(fd);
        written = true;
        hilog.info(DOMAIN, TAG, `Written directly to: ${projectFile}`);
      } catch (writeErr) {
        hilog.warn(DOMAIN, TAG, `Direct write failed (likely sandbox): ${writeErr}. Redirecting...`);
      }

      if (written) {
        this.addRecentProject(directoryPath);
        if (this.onProjectSelected) {
          this.onProjectSelected(directoryPath);
        }
        promptAction.showToast({ message: 'Project created', duration: 2000 });
      } else {
        // Fallback: create in sandbox
        let sandboxPath = this.createProjectInSandbox(fallbackName);
        if (sandboxPath.length > 0) {
          this.addRecentProject(sandboxPath);
          if (this.onProjectSelected) {
            this.onProjectSelected(sandboxPath);
          }
          promptAction.showToast({
            message: 'Created in app sandbox: ' + fallbackName,
            duration: 2000
          });
        }
      }
    } catch (e) {
      hilog.error(DOMAIN, TAG, `createAndOpenAt error: ${e}`);
      promptAction.showToast({ message: 'Failed to create project', duration: 2000 });
    }
  }

  /** ---- Recent Projects ---- */

  private openRecentProject(projectPath: string): void {
    hilog.info(DOMAIN, TAG, `Opening recent project: ${projectPath}`);
    this.validateAndOpen(projectPath);
  }

  private removeRecentProject(index: number): void {
    hilog.info(DOMAIN, TAG, `Removing recent project at index ${index}`);
    this.recentProjects.splice(index, 1);
    this.saveRecentProjects();
  }

  build() {
    Stack() {
      // Layer 1: Main content
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

      Text('Project Manager')
        .fontSize(20)
        .fontColor('#88FFFFFF')
        .margin({ bottom: 32 })

      // Actions
      Column() {
        Button() {
          Row() {
            Image($r('app.media.app_icon'))
              .width(24).height(24).margin({ right: 12 })
            Text('Open Project')
              .fontSize(16).fontWeight(FontWeight.Medium)
          }
        }
        .width(220).height(48)
        .backgroundColor('#FF6B6B').borderRadius(8)
        .onClick(() => this.browseForProject())

        Button() {
          Row() {
            Image($r('app.media.app_icon'))
              .width(24).height(24).margin({ right: 12 })
            Text('New Project')
              .fontSize(16).fontWeight(FontWeight.Medium)
          }
        }
        .width(220).height(48)
        .backgroundColor('#3A3A4A').borderRadius(8)
        .margin({ top: 12 })
        .onClick(() => this.showNewProjectInput())
      }
      .margin({ bottom: 40 })

      // Recent Projects
      if (this.recentProjects.length > 0) {
        Column() {
          Divider().color('#333344').margin({ bottom: 20 })
          Text('Recent Projects')
            .fontSize(14).fontColor('#666688')
            .margin({ bottom: 12 })

          List() {
            ForEach(this.recentProjects, (projectPath: string, index: number) => {
              ListItem() {
                Row() {
                  Text(projectPath)
                    .fontSize(14).fontColor('#CCCCCC')
                    .layoutWeight(1).maxLines(1)
                    .textOverflow({ overflow: TextOverflow.Ellipsis })

                  Button('X')
                    .width(28).height(28).fontSize(12)
                    .fontColor('#FF6B6B').backgroundColor('#00000000')
                    .onClick(() => this.removeRecentProject(index))
                }
                .width('100%')
                .padding({ left: 16, right: 8, top: 8, bottom: 8 })
              }
              .onClick(() => this.openRecentProject(projectPath))
              .backgroundColor('#252535').borderRadius(6)
              .margin({ bottom: 4 })
            }, (item: string, index: number) => index.toString())
          }
          .width('100%').constraintSize({ maxHeight: 200 })
        }
        .width('100%').constraintSize({ maxWidth: 500 })
        .padding({ left: 16, right: 16 })
      }
      }
      .width('100%').height('100%')
      .backgroundColor('#1E1E2E')
      .justifyContent(FlexAlign.Center)

      // Layer 2 + 3: Modal overlay (backdrop + dialog)
      if (this.showNewProjectDialog) {
        // Backdrop — blocks clicks to layer 1
        Column()
          .width('100%').height('100%')
          .backgroundColor('#00000088')
          .onClick(() => { /* absorb clicks, do nothing */ })
          .alignItems(HorizontalAlign.Center)
          .justifyContent(FlexAlign.Center)

        // Dialog — centered on top of backdrop
        Column() {
          Text('New Project')
            .fontSize(22)
            .fontWeight(FontWeight.Bold)
            .fontColor('#FFFFFF')
            .margin({ bottom: 20 })

          TextInput({ text: $$this.newProjectName, placeholder: 'Enter project name...' })
            .width('100%')
            .height(48)
            .fontSize(16)
            .backgroundColor('#2A2A3A')
            .borderRadius(8)
            .padding({ left: 16, right: 16 })
            .margin({ bottom: 20 })

          Row() {
            Button('Cancel')
              .width(100).height(40)
              .backgroundColor('#444455').fontColor('#CCCCCC')
              .borderRadius(6)
              .onClick(() => this.cancelNewProject())

            Blank().width(16)

            Button('Create')
              .width(100).height(40)
              .backgroundColor('#FF6B6B').fontColor('#FFFFFF')
              .borderRadius(6)
              .onClick(() => this.confirmNewProject())
          }
        }
        .width(340)
        .padding(24)
        .backgroundColor('#2A2A3C')
        .borderRadius(12)
        .shadow({ radius: 20, color: '#00000066' })
      }
    }
    .width('100%').height('100%')
  }
}
