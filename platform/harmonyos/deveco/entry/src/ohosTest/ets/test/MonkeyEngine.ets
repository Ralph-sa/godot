/**
 * MonkeyEngine — 随机事件生成引擎
 *
 * 为 Godot HarmonyOS 编辑器量身定制的 Monkey 压力测试核心。
 * 支持以下事件类型:
 *   - TOUCH     : 随机坐标点击/滑动
 *   - KEY       : 随机键盘按键
 *   - MOUSE     : 随机鼠标移动/点击/滚轮
 *   - PAUSE_RESUME : 模拟前后台切换
 *   - BACK      : 返回键
 *
 * 用法:
 *   const monkey = new MonkeyEngine({ iterations: 1000, seed: 42 });
 *   monkey.onCrash = (reason) => { /* 记录崩溃 */ };
 *   await monkey.run();
 *   console.log(monkey.summary());
 */
import { hilog } from '@kit.PerformanceAnalysisKit';

const TAG = 'MonkeyEngine';
const DOMAIN = 0x0000;

// ── 事件类型枚举 ──
export enum MonkeyEventType {
  TOUCH = 'TOUCH',
  KEY = 'KEY',
  MOUSE = 'MOUSE',
  PAUSE_RESUME = 'PAUSE_RESUME',
  BACK = 'BACK',
  IDLE = 'IDLE'
}

// ── 事件描述 ──
export interface MonkeyEvent {
  type: MonkeyEventType;
  timestamp: number;
  data: Record<string, number | string>;
}

// ── 引擎配置 ──
export interface MonkeyConfig {
  iterations: number;           // 总事件数（默认 500）
  seed: number;                 // 随机种子（可复现）
  maxDurationMs: number;        // 最大运行时间（默认 30000ms）
  eventIntervalMs: number;      // 事件间隔（默认 50ms）

  // 各事件类型权重（总和不必为 1，引擎会自动归一化）
  weightTouch: number;          // 默认 40
  weightKey: number;            // 默认 15
  weightMouse: number;          // 默认 25
  weightPauseResume: number;    // 默认 5
  weightBack: number;           // 默认 5
  weightIdle: number;           // 默认 10

  // 屏幕尺寸（用于随机坐标范围）
  screenWidth: number;
  screenHeight: number;

  // 目标应用 Bundle Name（用于监控存活）
  targetBundle: string;
}

// ── 运行统计 ──
export interface MonkeySummary {
  totalEvents: number;
  elapsedMs: number;
  eventsByType: Record<string, number>;
  crashes: number;
  crashReasons: string[];
  errors: number;
  errorMessages: string[];
  startTime: number;
  endTime: number;
}

// ── 事件回调 ──
export interface MonkeyCallbacks {
  /** 每个事件执行前调用 */
  onBeforeEvent?: (event: MonkeyEvent) => void;
  /** 每个事件执行后调用 */
  onAfterEvent?: (event: MonkeyEvent) => void;
  /** 检测到崩溃时调用 */
  onCrash?: (reason: string) => void;
  /** 检测到错误时调用 */
  onError?: (message: string) => void;
  /** 进度回调 (current, total) */
  onProgress?: (current: number, total: number) => void;
}

// ── 简易伪随机数生成器 (Mulberry32) 可复现 ──
class SeededRandom {
  private state: number;

  constructor(seed: number) {
    this.state = seed | 0;
  }

  /** 返回 [0, 1) 的浮点数 */
  next(): number {
    this.state = (this.state + 0x6D2B79F5) | 0;
    let t: number = this.state;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  }

  /** 返回 [min, max) 的随机整数 */
  nextInt(min: number, max: number): number {
    return Math.floor(this.next() * (max - min) + min);
  }

  /** 返回 [min, max) 的随机浮点数 */
  nextFloat(min: number, max: number): number {
    return this.next() * (max - min) + min;
  }
}

// ── Monkey 引擎主体 ──
export class MonkeyEngine {
  private config: MonkeyConfig;
  private rng: SeededRandom;
  private events: MonkeyEvent[] = [];
  private startTime: number = 0;

  // 统计
  private crashCount: number = 0;
  private crashReasons: string[] = [];
  private errorCount: number = 0;
  private errorMessages: string[] = [];
  private eventCounts: Record<string, number> = {};

  // 回调
  public callbacks: MonkeyCallbacks = {};

  // 事件注入函数（由外部设置，因为依赖 UiTest / NAPI 桥接）
  public injectEvent?: (event: MonkeyEvent) => Promise<void>;
  public checkAppAlive?: () => Promise<boolean>;

  constructor(config: Partial<MonkeyConfig> = {}) {
    this.config = {
      iterations: 500,
      seed: Date.now(),
      maxDurationMs: 30000,
      eventIntervalMs: 50,
      weightTouch: 40,
      weightKey: 15,
      weightMouse: 25,
      weightPauseResume: 5,
      weightBack: 5,
      weightIdle: 10,
      screenWidth: 1920,
      screenHeight: 1080,
      targetBundle: 'com.godotengine.editor',
      ...config
    };
    this.rng = new SeededRandom(this.config.seed);

    // 初始化计数器
    for (const t of Object.values(MonkeyEventType)) {
      this.eventCounts[t] = 0;
    }

    hilog.info(DOMAIN, TAG, `MonkeyEngine created: seed=${this.config.seed}, ` +
      `iterations=${this.config.iterations}`);
  }

  // ── 事件生成 ──

  /** 基于权重随机选择事件类型 */
  private selectEventType(): MonkeyEventType {
    const w = this.config;
    const totalWeight: number =
      w.weightTouch + w.weightKey + w.weightMouse +
      w.weightPauseResume + w.weightBack + w.weightIdle;

    let roll: number = this.rng.nextFloat(0, totalWeight);
    let cumulative: number = 0;

    cumulative += w.weightTouch;
    if (roll < cumulative) return MonkeyEventType.TOUCH;

    cumulative += w.weightKey;
    if (roll < cumulative) return MonkeyEventType.KEY;

    cumulative += w.weightMouse;
    if (roll < cumulative) return MonkeyEventType.MOUSE;

    cumulative += w.weightPauseResume;
    if (roll < cumulative) return MonkeyEventType.PAUSE_RESUME;

    cumulative += w.weightBack;
    if (roll < cumulative) return MonkeyEventType.BACK;

    return MonkeyEventType.IDLE;
  }

  /** 生成触摸事件数据 */
  private generateTouchEvent(): MonkeyEvent {
    const x: number = this.rng.nextFloat(0, this.config.screenWidth);
    const y: number = this.rng.nextFloat(0, this.config.screenHeight);
    const action: number = this.rng.nextInt(0, 3); // 0=down, 1=up, 2=move
    const touchId: number = this.rng.nextInt(0, 5);
    return {
      type: MonkeyEventType.TOUCH,
      timestamp: Date.now(),
      data: { touchId, action, x, y }
    };
  }

  /** 生成键盘事件数据 */
  private generateKeyEvent(): MonkeyEvent {
    // 常用 HarmonyOS 键码范围
    const commonKeys: number[] = [
      2014, // Escape
      2017, // ArrowUp
      2018, // ArrowDown
      2019, // ArrowLeft
      2020, // ArrowRight
      2003, // Enter
      2004, // Backspace
      2005, // Tab
      2006, // Space
      2050, // F5
    ];
    const keyCode: number = commonKeys[this.rng.nextInt(0, commonKeys.length)];
    const eventType: number = this.rng.nextInt(0, 2); // 0=down, 1=up
    return {
      type: MonkeyEventType.KEY,
      timestamp: Date.now(),
      data: { keyCode, eventType, keyText: `key_${keyCode}` }
    };
  }

  /** 生成鼠标事件数据 */
  private generateMouseEvent(): MonkeyEvent {
    const x: number = this.rng.nextFloat(0, this.config.screenWidth);
    const y: number = this.rng.nextFloat(0, this.config.screenHeight);
    const button: number = this.rng.nextInt(0, 3); // 0=left, 1=right, 2=middle
    const action: number = this.rng.nextInt(0, 3); // 0=press, 1=release, 2=move
    const offsetX: number = this.rng.nextFloat(-10, 10);
    const offsetY: number = this.rng.nextFloat(-10, 10);
    return {
      type: MonkeyEventType.MOUSE,
      timestamp: Date.now(),
      data: { button, action, x, y, offsetX, offsetY }
    };
  }

  /** 生成暂停/恢复事件 */
  private generatePauseResumeEvent(): MonkeyEvent {
    const action: number = this.rng.nextInt(0, 2); // 0=pause, 1=resume
    const delayMs: number = this.rng.nextInt(100, 2000);
    return {
      type: MonkeyEventType.PAUSE_RESUME,
      timestamp: Date.now(),
      data: { action, delayMs }
    };
  }

  // ── 主运行循环 ──

  async run(): Promise<MonkeySummary> {
    this.startTime = Date.now();
    hilog.info(DOMAIN, TAG, 'MonkeyEngine starting...');

    for (let i = 0; i < this.config.iterations; i++) {
      // 检查超时
      if (Date.now() - this.startTime > this.config.maxDurationMs) {
        hilog.warn(DOMAIN, TAG, `MonkeyEngine: max duration reached at iteration ${i}`);
        break;
      }

      // 检查应用是否还活着
      if (this.checkAppAlive) {
        const alive: boolean = await this.checkAppAlive();
        if (!alive) {
          const reason: string = `App died at iteration ${i}`;
          this.crashCount++;
          this.crashReasons.push(reason);
          if (this.callbacks.onCrash) {
            this.callbacks.onCrash(reason);
          }
          hilog.error(DOMAIN, TAG, reason);
          break;
        }
      }

      // 生成事件
      const eventType: MonkeyEventType = this.selectEventType();
      let event: MonkeyEvent;

      switch (eventType) {
        case MonkeyEventType.TOUCH:
          event = this.generateTouchEvent();
          break;
        case MonkeyEventType.KEY:
          event = this.generateKeyEvent();
          break;
        case MonkeyEventType.MOUSE:
          event = this.generateMouseEvent();
          break;
        case MonkeyEventType.PAUSE_RESUME:
          event = this.generatePauseResumeEvent();
          break;
        case MonkeyEventType.BACK:
          event = { type: MonkeyEventType.BACK, timestamp: Date.now(), data: {} };
          break;
        default:
          event = { type: MonkeyEventType.IDLE, timestamp: Date.now(), data: {} };
      }

      this.events.push(event);
      this.eventCounts[event.type] = (this.eventCounts[event.type] || 0) + 1;

      // 回调：事件前
      if (this.callbacks.onBeforeEvent) {
        this.callbacks.onBeforeEvent(event);
      }

      // 注入事件
      try {
        if (this.injectEvent) {
          await this.injectEvent(event);
        }
      } catch (e) {
        const msg: string = `Event injection failed at iter ${i}: ${e}`;
        this.errorCount++;
        this.errorMessages.push(msg);
        if (this.callbacks.onError) {
          this.callbacks.onError(msg);
        }
        hilog.error(DOMAIN, TAG, msg);
      }

      // 回调：事件后
      if (this.callbacks.onAfterEvent) {
        this.callbacks.onAfterEvent(event);
      }

      // 进度报告（每 50 个事件报告一次）
      if (this.callbacks.onProgress && i % 50 === 0) {
        this.callbacks.onProgress(i + 1, this.config.iterations);
      }

      // 事件间隔（IDLE 事件可以适当延长）
      const interval: number = eventType === MonkeyEventType.IDLE
        ? this.config.eventIntervalMs * 3
        : this.config.eventIntervalMs;
      await this.sleep(interval);
    }

    const endTime: number = Date.now();
    hilog.info(DOMAIN, TAG, 'MonkeyEngine finished');
    return this.buildSummary(endTime);
  }

  /** 构建运行摘要 */
  private buildSummary(endTime: number): MonkeySummary {
    return {
      totalEvents: this.events.length,
      elapsedMs: endTime - this.startTime,
      eventsByType: { ...this.eventCounts },
      crashes: this.crashCount,
      crashReasons: [...this.crashReasons],
      errors: this.errorCount,
      errorMessages: [...this.errorMessages],
      startTime: this.startTime,
      endTime
    };
  }

  /** 获取当前摘要（运行时可用） */
  summary(): MonkeySummary {
    return this.buildSummary(Date.now());
  }

  /** 获取完整事件记录（用于回放） */
  getEventLog(): MonkeyEvent[] {
    return [...this.events];
  }

  private sleep(ms: number): Promise<void> {
    return new Promise<void>((resolve) => {
      setTimeout(resolve, ms);
    });
  }
}
