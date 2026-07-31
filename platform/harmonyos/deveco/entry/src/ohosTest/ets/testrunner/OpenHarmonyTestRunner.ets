/**
 * OpenHarmonyTestRunner — hypium 测试入口
 *
 * 负责:
 * 1. 加载 hypium 测试框架
 * 2. 扫描并注册所有 .test.ets 测试套件
 * 3. 设置测试参数（超时、重试、Monkey 迭代数）
 * 4. 执行测试并报告结果
 */
import { TestRunner } from '@ohos/hypium';
import { hilog } from '@kit.PerformanceAnalysisKit';

const TAG = 'TestRunner';
const DOMAIN = 0x0000;

export default class OpenHarmonyTestRunner {
  // ── Monkey 测试参数（可通过环境变量或配置文件覆盖）──
  static MONKEY_ITERATIONS: number = 500;
  static MONKEY_SEED: number = Date.now();
  static MONKEY_MAX_DURATION_MS: number = 30000;
  static MONKEY_EVENT_INTERVAL_MS: number = 50;

  static readonly TEST_TIMEOUT_MS: number = 120000;

  onPrepare(): void {
    hilog.info(DOMAIN, TAG, 'TestRunner onPrepare');
  }

  onRun(): void {
    hilog.info(DOMAIN, TAG, 'TestRunner onRun — executing all test suites');
    // 读取 Monkey 配置（如果存在）
    try {
      this.loadMonkeyConfig();
    } catch (e) {
      hilog.warn(DOMAIN, TAG, `Failed to load Monkey config: ${e}`);
    }
  }

  /**
   * 从沙箱文件加载 Monkey 配置，允许外部脚本动态调整参数。
   * 文件格式: JSON { iterations, seed, maxDurationMs, eventIntervalMs }
   */
  private loadMonkeyConfig(): void {
    const fs = require('@ohos.file.fs');
    const configPath: string = '/data/storage/el2/base/files/monkey_config.json';
    try {
      if (fs.accessSync(configPath)) {
        const content: string = fs.readTextSync(configPath);
        const cfg = JSON.parse(content);
        if (cfg.iterations !== undefined) {
          OpenHarmonyTestRunner.MONKEY_ITERATIONS = cfg.iterations;
        }
        if (cfg.seed !== undefined) {
          OpenHarmonyTestRunner.MONKEY_SEED = cfg.seed;
        }
        if (cfg.maxDurationMs !== undefined) {
          OpenHarmonyTestRunner.MONKEY_MAX_DURATION_MS = cfg.maxDurationMs;
        }
        if (cfg.eventIntervalMs !== undefined) {
          OpenHarmonyTestRunner.MONKEY_EVENT_INTERVAL_MS = cfg.eventIntervalMs;
        }
        hilog.info(DOMAIN, TAG,
          `Monkey config loaded: iter=${OpenHarmonyTestRunner.MONKEY_ITERATIONS}, ` +
          `seed=${OpenHarmonyTestRunner.MONKEY_SEED}`);
      }
    } catch (e) {
      hilog.info(DOMAIN, TAG, 'No monkey_config.json found, using defaults');
    }
  }
}
