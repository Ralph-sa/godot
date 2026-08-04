/**
 * ProjectSelector.ets - Native project manager for Godot HarmonyOS
 *
 * "Open Project" uses system file picker to locate existing projects.
 * "New Project" workflow: pick a parent folder → enter project name → create.
 * Adaptive layout: uses constraintSize so UI looks good in both
 * fullscreen and floating-window modes.
 */

import { common } from '@kit.AbilityKit';
import { fileIo } from '@kit.CoreFileKit';
import { picker } from '@kit.CoreFileKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { godot_napi } from 'libgodot_napi.so';

const TAG = 'ProjectSelector';
const DOMAIN = 0x0000;

export type ProjectSelectCallback = (projectPath: string) => void;

interface DialogButton {
  text: string;
  color: string;
}

interface DialogResult {
  index: number;
}

@ComponentV2
export struct ProjectSelector {
  @Param @Require onProjectSelected?: ProjectSelectCallback;
  @Local recentProjects: string[] = [];
  @Local showNewProjectDialog: boolean = false;
  @Local newProjectName: string = '';
  @Local selectedFolder: string = '';
  private importEntryCount: number = 0;

  aboutToAppear(): void {
    hilog.info(DOMAIN, TAG, 'ProjectSelector aboutToAppear');
    this.loadRecentProjects();
  }

  /** ---- Helpers (context / prompt) ---- */

  private getCtx(): common.UIAbilityContext {
    return this.getUIContext().getHostContext() as common.UIAbilityContext;
  }

  private showToast(message: string, duration: number = 2000): void {
    try {
      this.getUIContext().getPromptAction().showToast({ message, duration });
    } catch (e) {
      hilog.warn(DOMAIN, TAG, `Toast failed: ${e}`);
    }
  }

  private showDialog(
    title: string,
    message: string,
    buttons: DialogButton[]
  ): Promise<DialogResult> {
    try {
      return this.getUIContext().getPromptAction().showDialog({
        title, message, buttons
      }) as Promise<DialogResult>;
    } catch (e) {
      hilog.error(DOMAIN, TAG, `showDialog failed: ${e}`);
      let fallback: DialogResult = { index: -1 };
      return Promise.resolve(fallback);
    }
  }

  /** ---- Recent Projects Persistence ---- */

  private loadRecentProjects(): void {
    try {
      let prefsPath: string = this.getCtx().filesDir + '/godot_recent_projects.json';
      hilog.info(DOMAIN, TAG, `Loading recent projects from: ${prefsPath}`);
      if (fileIo.accessSync(prefsPath)) {
        let content: string = fileIo.readTextSync(prefsPath);
        let loaded: string[] = JSON.parse(content) as string[];
        this.recentProjects = [];
        for (let index = 0; index < loaded.length && this.recentProjects.length < 10; index++) {
          let projectPath = loaded[index];
          if (this.isSandboxProjectPath(projectPath) &&
            fileIo.accessSync(projectPath + '/project.godot')) {
            this.recentProjects.push(projectPath);
          } else {
            hilog.warn(DOMAIN, TAG, 'Discarding unsafe or missing recent project: ' + projectPath);
          }
        }
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
      let prefsPath: string = this.getCtx().filesDir + '/godot_recent_projects.json';
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
    if (!this.isSandboxProjectPath(projectPath) ||
      !fileIo.accessSync(projectPath + '/project.godot')) {
      hilog.error(DOMAIN, TAG, 'Refusing non-sandbox recent project: ' + projectPath);
      return;
    }
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

  /** ---- Sandbox Project Creation ---- */

  private getSandboxProjectsDir(): string {
    let dir = this.getCtx().filesDir + '/projects';
    hilog.info(DOMAIN, TAG, `Sandbox projects dir: ${dir}`);
    return dir;
  }

  private assertSandboxDirectoryChain(path: string, allowMissingLeaf: boolean = false): void {
    const filesDir: string = this.getCtx().filesDir;
    if (path !== filesDir && !path.startsWith(filesDir + '/')) {
      throw new Error('Path is outside filesDir: ' + path);
    }
    if (path.includes('/../') || path.endsWith('/..') ||
      path.includes('/./') || path.endsWith('/.') ||
      path.includes('\\') || path.includes('\0')) {
      throw new Error('Unsafe sandbox path: ' + path);
    }

    let filesStat = fileIo.lstatSync(filesDir);
    if (filesStat.isSymbolicLink() || !filesStat.isDirectory()) {
      throw new Error('filesDir is not a real directory');
    }

    let relative: string = path === filesDir ? '' : path.substring(filesDir.length + 1);
    let segments: string[] = relative.length > 0 ? relative.split('/') : [];
    let current: string = filesDir;
    for (let index = 0; index < segments.length; index++) {
      let segment: string = segments[index];
      if (!this.isSafeEntryName(segment)) {
        throw new Error('Unsafe sandbox path segment: ' + segment);
      }
      current += '/' + segment;
      if (!fileIo.accessSync(current)) {
        if (allowMissingLeaf && index === segments.length - 1) {
          return;
        }
        throw new Error('Sandbox directory is missing: ' + current);
      }
      let stat = fileIo.lstatSync(current);
      if (stat.isSymbolicLink() || !stat.isDirectory()) {
        throw new Error('Sandbox directory chain contains a non-directory or symbolic link: ' + current);
      }
    }
  }

  private ensureSandboxProjectsDir(): string {
    let projectsDir: string = this.getSandboxProjectsDir();
    this.assertSandboxDirectoryChain(projectsDir, true);
    if (!fileIo.accessSync(projectsDir)) {
      try {
        // filesDir already exists; recursive creation would follow an
        // unexpectedly replaced parent and weakens the postcondition.
        fileIo.mkdirSync(projectsDir, false);
      } catch (e) {
        if (!fileIo.accessSync(projectsDir)) {
          throw new Error('Failed to create projects directory: ' + e);
        }
      }
    }
    // Re-check after creation so an existing or concurrently replaced entry
    // cannot be accepted as the projects root.
    this.assertSandboxDirectoryChain(projectsDir, false);
    return projectsDir;
  }

  private isSandboxProjectPath(projectPath: string): boolean {
    let projectsDir = this.getSandboxProjectsDir();
    return projectPath.startsWith(projectsDir + '/') &&
      !projectPath.includes('/../') && !projectPath.endsWith('/..') &&
      !projectPath.includes('/./') && !projectPath.endsWith('/.') &&
      !projectPath.includes('\\') && !projectPath.includes('\0');
  }

  /**
   * Create a new Godot project in the app sandbox.
   * Returns the project path on success, empty string on failure.
   */
  private createProjectInSandbox(projectName: string): string {
    hilog.info(DOMAIN, TAG, '[INIT STEP 2/16] createProjectInSandbox: ' + projectName);

    try {
      if (!this.isSafeEntryName(projectName)) {
        throw new Error('Unsafe project name');
      }
      let projectsDir = this.ensureSandboxProjectsDir();
      let projectDir = projectsDir + '/' + projectName;

      // Check if project already exists
      if (fileIo.accessSync(projectDir)) {
        hilog.warn(DOMAIN, TAG, 'Project directory already exists: ' + projectDir);
        this.showToast('Project already exists!');
        return '';
      }

      // Create project directory
      this.assertSandboxDirectoryChain(projectDir, true);
      fileIo.mkdirSync(projectDir, false);
      this.assertSandboxDirectoryChain(projectDir, false);
      hilog.info(DOMAIN, TAG, '[INIT STEP 2/16] Project dir created: ' + projectDir);

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
      if (fileIo.accessSync(projectFile)) {
        throw new Error('project.godot unexpectedly exists before creation');
      }
      let fd = fileIo.openSync(projectFile, fileIo.OpenMode.CREATE |
        fileIo.OpenMode.WRITE_ONLY | fileIo.OpenMode.TRUNC);
      fileIo.writeSync(fd.fd, projectConfig);
      fileIo.closeSync(fd);
      let projectFileStat = fileIo.lstatSync(projectFile);
      if (projectFileStat.isSymbolicLink() || !projectFileStat.isFile()) {
        throw new Error('project.godot was not created as a regular file');
      }

      hilog.info(DOMAIN, TAG, '[INIT STEP 2/16] project.godot written: ' + projectFile);
      return projectDir;
    } catch (e) {
      hilog.error(DOMAIN, TAG, `createProjectInSandbox error: ${e}`);
      this.showToast('Failed to create project');
      return '';
    }
  }

  /** ---- New Project Dialog (folder → name → create) ---- */

  private showNewProjectInput(): void {
    hilog.info(DOMAIN, TAG, '[INIT STEP 1/16] Showing new project dialog');
    this.selectedFolder = this.getSandboxProjectsDir();
    this.newProjectName = '';
    this.showNewProjectDialog = true;
  }

  private confirmNewProject(): void {
    hilog.info(DOMAIN, TAG, '[INIT STEP 2/16] confirmNewProject: "' + this.newProjectName + '" in ' + this.selectedFolder);
    this.showNewProjectDialog = false;

    let name = this.newProjectName.trim();
    if (name.length === 0) {
      hilog.warn(DOMAIN, TAG, 'Project name is empty');
      this.showToast('Please enter a project name');
      return;
    }

    // Sanitize name: replace spaces and special chars
    let safeName = name.replace(/[^a-zA-Z0-9_\- ]/g, '').replace(/\s+/g, '_');
    if (safeName.length === 0) {
      safeName = 'NewProject';
    }

    // Native Godot consumes POSIX paths, while the system picker returns
    // document URIs. New projects therefore always live under filesDir so the
    // exact path passed to libgodot is both writable and directly resolvable.
    let projectPath = this.createProjectInSandbox(safeName);

    if (projectPath.length > 0) {
      this.addRecentProject(projectPath);
      if (this.onProjectSelected) {
        hilog.info(DOMAIN, TAG, '[INIT STEP 3/16] Firing onProjectSelected: ' + projectPath);
        this.onProjectSelected(projectPath);
      }
      this.showToast('Project created: ' + safeName);
    }
  }

  private cancelNewProject(): void {
    hilog.info(DOMAIN, TAG, 'New project cancelled');
    this.showNewProjectDialog = false;
    this.newProjectName = '';
    this.selectedFolder = '';
  }

  /** ---- Open Existing Project ---- */

  private joinDocumentUri(base: string, name: string): string {
    return base.endsWith('/') ? base + name : base + '/' + name;
  }

  private projectNameFromUri(uri: string): string {
    let trimmed = uri.endsWith('/') ? uri.substring(0, uri.length - 1) : uri;
    let lastSlash = trimmed.lastIndexOf('/');
    let rawName = lastSlash >= 0 ? trimmed.substring(lastSlash + 1) : trimmed;
    let decodedName = rawName;
    try {
      decodedName = decodeURIComponent(rawName);
    } catch (_e) {
      // Keep the provider-supplied segment when percent decoding fails.
    }
    let safeName = decodedName.replace(/[^a-zA-Z0-9_\- ]/g, '').replace(/\s+/g, '_');
    return safeName.length > 0 ? safeName : 'ImportedProject';
  }

  private isSafeEntryName(name: string): boolean {
    return name.length > 0 && name.length <= 255 &&
      name !== '.' && name !== '..' &&
      !name.includes('/') && !name.includes('\\') && !name.includes('\0');
  }

  private copyProjectTree(source: string, destination: string, depth: number): void {
    if (depth > 64) {
      throw new Error('Project directory nesting exceeds 64 levels');
    }
    this.assertSandboxDirectoryChain(destination, false);

    let entries: string[] = fileIo.listFileSync(source);
    for (let index = 0; index < entries.length; index++) {
      let name = entries[index];
      if (!this.isSafeEntryName(name)) {
        throw new Error('Unsafe project entry name: ' + name);
      }
      if (name === '.git' || name === '.godot') {
        continue;
      }

      this.importEntryCount++;
      if (this.importEntryCount > 50000) {
        throw new Error('Project contains more than 50000 entries');
      }

      let sourceEntry = this.joinDocumentUri(source, name);
      let destinationEntry = destination + '/' + name;
      if (fileIo.accessSync(destinationEntry)) {
        throw new Error('Import destination unexpectedly exists: ' + name);
      }
      let stat = fileIo.lstatSync(sourceEntry);
      if (stat.isSymbolicLink()) {
        throw new Error('Symbolic links are not supported in imported projects: ' + name);
      }
      if (stat.isDirectory()) {
        this.assertSandboxDirectoryChain(destinationEntry, true);
        fileIo.mkdirSync(destinationEntry, false);
        this.assertSandboxDirectoryChain(destinationEntry, false);
        this.copyProjectTree(sourceEntry, destinationEntry, depth + 1);
      } else if (stat.isFile()) {
        fileIo.copyFileSync(sourceEntry, destinationEntry);
        let destinationStat = fileIo.lstatSync(destinationEntry);
        if (destinationStat.isSymbolicLink() || !destinationStat.isFile()) {
          throw new Error('Imported entry is not a regular file: ' + name);
        }
      }
    }
  }

  private importAndOpenProject(sourceUri: string): void {
    hilog.info(DOMAIN, TAG, 'Importing selected project URI: ' + sourceUri);
    let stagingDestination: string = '';
    let stagingLeaf: string = '';
    try {
      let sourceProjectFile = this.joinDocumentUri(sourceUri, 'project.godot');
      if (!fileIo.accessSync(sourceProjectFile)) {
        this.showToast('Selected folder has no project.godot');
        hilog.warn(DOMAIN, TAG, 'Selected folder is not a Godot project: ' + sourceUri);
        return;
      }

      let projectsDir = this.ensureSandboxProjectsDir();

      let baseName = this.projectNameFromUri(sourceUri);
      if (!this.isSafeEntryName(baseName)) {
        throw new Error('Unsafe imported project name');
      }
      let destinationLeaf: string = baseName;
      let destination = projectsDir + '/' + destinationLeaf;
      let suffix = 1;
      while (fileIo.accessSync(destination)) {
        destinationLeaf = baseName + '_import_' + suffix.toString();
        destination = projectsDir + '/' + destinationLeaf;
        suffix++;
      }

      stagingLeaf = destinationLeaf + '.importing';
      stagingDestination = projectsDir + '/' + stagingLeaf;
      while (fileIo.accessSync(stagingDestination)) {
        stagingLeaf += '_';
        stagingDestination = projectsDir + '/' + stagingLeaf;
      }
      if (!this.isSafeEntryName(stagingLeaf)) {
        throw new Error('Unsafe staging directory name');
      }
      this.assertSandboxDirectoryChain(stagingDestination, true);
      fileIo.mkdirSync(stagingDestination, false);
      this.assertSandboxDirectoryChain(stagingDestination, false);
      this.importEntryCount = 0;
      this.copyProjectTree(sourceUri, stagingDestination, 0);
      if (!fileIo.accessSync(stagingDestination + '/project.godot')) {
        throw new Error('Imported project.godot is missing');
      }
      // Native code opens filesDir and projects with one O_NOFOLLOW dirfd chain,
      // validates project.godot, and uses renameat2(RENAME_NOREPLACE). ArkTS
      // never performs the final transaction by path.
      this.assertSandboxDirectoryChain(projectsDir, false);
      this.assertSandboxDirectoryChain(stagingDestination, false);
      let commitResult: number = godot_napi.commitProjectImport(
        this.getCtx().filesDir, stagingLeaf, destinationLeaf);
      if (commitResult !== 0) {
        throw new Error('Native project import commit failed: ' + commitResult.toString());
      }
      this.assertSandboxDirectoryChain(destination, false);
      stagingDestination = '';
      stagingLeaf = '';

      hilog.info(DOMAIN, TAG, 'Project imported into sandbox: ' + destination +
        ', entries=' + this.importEntryCount.toString());
      this.addRecentProject(destination);
      if (this.onProjectSelected) {
        this.onProjectSelected(destination);
      }
      this.showToast('Project imported into app sandbox');
    } catch (e) {
      if (stagingLeaf.length > 0) {
        try {
          let cleanupResult: number = godot_napi.cleanupProjectImport(
            this.getCtx().filesDir, stagingLeaf);
          if (cleanupResult !== 0) {
            throw new Error('Native staging cleanup failed: ' + cleanupResult.toString());
          }
        } catch (cleanupError) {
          hilog.warn(DOMAIN, TAG, 'Failed to clean staging import: ' + cleanupError);
        }
      }
      hilog.error(DOMAIN, TAG, 'Project import failed: ' + e);
      this.showToast('Failed to import selected project');
    }
  }

  private async browseForProject(): Promise<void> {
    hilog.info(DOMAIN, TAG, 'Opening file picker');
    try {
      let documentPicker = new picker.DocumentViewPicker(this.getCtx());
      let options = new picker.DocumentSelectOptions();
      options.maxSelectNumber = 1;
      options.selectMode = picker.DocumentSelectMode.FOLDER;

      documentPicker.select(options).then((uris: string[]) => {
        if (uris && uris.length > 0) {
          let selectedUri = uris[0];
          hilog.info(DOMAIN, TAG, `File picker returned: ${selectedUri}`);
          this.importAndOpenProject(selectedUri);
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
    hilog.info(DOMAIN, TAG, 'Validating: ' + directoryPath);
    try {
      if (!this.isSandboxProjectPath(directoryPath)) {
        hilog.error(DOMAIN, TAG, 'Rejected recent project outside filesDir: ' + directoryPath);
        this.showToast('Project must be imported into the app sandbox');
        return;
      }
      let projectFile: string = directoryPath + '/project.godot';

      if (fileIo.accessSync(projectFile)) {
        hilog.info(DOMAIN, TAG, `Valid Godot project found: ${directoryPath}`);
        this.addRecentProject(directoryPath);
        if (this.onProjectSelected) {
          this.onProjectSelected(directoryPath);
        }
        this.showToast('Project opened');
      } else {
        hilog.warn(DOMAIN, TAG, 'Recent project is missing project.godot: ' + directoryPath);
        this.showToast('Recent project is missing or incomplete');
      }
    } catch (e) {
      hilog.error(DOMAIN, TAG, `Validate error: ${e}`);
      this.showToast('Failed to access directory');
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

  /* ==================== Build / UI ==================== */

  build() {
    Stack() {
      // ---- Layer 1: Main content (responsive) ----
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

        // Action buttons — adaptive width
        Column() {
          Button() {
            Row() {
              Image($r('app.media.app_icon'))
                .width(24).height(24).margin({ right: 12 })
              Text('Open Project')
                .fontSize(16).fontWeight(FontWeight.Medium)
                .fontColor('#FFFFFF')
            }
          }
          .width('100%').height(48)
          .constraintSize({ maxWidth: 320 })
          .backgroundColor('#FF6B6B').borderRadius(8)
          .onClick(() => this.browseForProject())

          Button() {
            Row() {
              Image($r('app.media.app_icon'))
                .width(24).height(24).margin({ right: 12 })
              Text('New Project')
                .fontSize(16).fontWeight(FontWeight.Medium)
                .fontColor('#E0E0E0')
            }
          }
          .width('100%').height(48)
          .constraintSize({ maxWidth: 320 })
          .backgroundColor('#3A3A4A').borderRadius(8)
          .margin({ top: 12 })
          .onClick(() => this.showNewProjectInput())
        }
        .width('85%')
        .constraintSize({ maxWidth: 320 })
        .margin({ bottom: 40 })

        // Recent Projects — responsive
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
          .width('90%')
          .constraintSize({ maxWidth: 600 })
        }
      }
      .width('100%').height('100%')
      .backgroundColor('#1E1E2E')
      .justifyContent(FlexAlign.Center)

      // ---- Layer 2 + 3: Modal overlay (backdrop + dialog) ----
      if (this.showNewProjectDialog) {
        // Backdrop — blocks clicks to layer 1
        Column()
          .width('100%').height('100%')
          .backgroundColor('#00000088')
          .onClick(() => { /* absorb clicks, do nothing */ })
          .alignItems(HorizontalAlign.Center)
          .justifyContent(FlexAlign.Center)

        // Dialog — responsive, centered on top of backdrop
        Column() {
          Text('New Project')
            .fontSize(22)
            .fontWeight(FontWeight.Bold)
            .fontColor('#FFFFFF')
            .margin({ bottom: 16 })

          // Parent folder selection
          Column() {
            Text('Parent folder:')
              .fontSize(11)
              .fontColor('#666688')
              .margin({ bottom: 4 })

            Row() {
              Text(this.selectedFolder)
                .fontSize(11)
                .fontColor('#888899')
                .layoutWeight(1)
                .maxLines(2)
                .textOverflow({ overflow: TextOverflow.Ellipsis })

              Button('Sandbox only')
                .height(32)
                .fontSize(11)
                .backgroundColor('#444455')
                .fontColor('#CCCCCC')
                .borderRadius(4)
                .padding({ left: 8, right: 8 })
                .enabled(false)
            }
            .width('100%')
          }
          .width('100%')
          .backgroundColor('#1E1E30')
          .borderRadius(6)
          .padding(10)
          .margin({ bottom: 16 })

          TextInput({ text: $$this.newProjectName, placeholder: 'Enter project name...' })
            .width('100%')
            .height(48)
            .fontSize(16)
            .fontColor('#FFFFFF')
            .placeholderColor('#666688')
            .placeholderFont({ size: 16 })
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
        .width('85%')
        .constraintSize({ maxWidth: 400 })
        .padding(24)
        .backgroundColor('#2A2A3C')
        .borderRadius(12)
        .shadow({ radius: 20, color: '#00000066' })
      }
    }
    .width('100%').height('100%')
  }
}
