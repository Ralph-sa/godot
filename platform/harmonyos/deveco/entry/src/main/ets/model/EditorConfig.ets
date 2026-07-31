/**
 * EditorConfig.ets - Editor configuration model
 */
export class EditorConfig {
  /** Target rendering driver */
  renderingDriver: string = 'vulkan';

  /** Display width in pixels */
  displayWidth: number = 1920;

  /** Display height in pixels */
  displayHeight: number = 1080;

  /** VSync mode: 0=disabled, 1=enabled, 2=adaptive */
  vsyncMode: number = 1;

  /** Whether to keep screen on */
  keepScreenOn: boolean = true;
}
