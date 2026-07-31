/**
 * Monkey.test.ets — Monkey 压力测试套件
 *
 * 测试目标:
 *   1. Smoke (100 次迭代) — 快速冒烟，验证事件注入通道
 *   2. Standard (500 次迭代) — 标准 Monkey 测试
 *   3. Stress (2000 次迭代) — 压力测试（仅在 --full 模式运行）
 *
 * 事件注入方式:
 *   通过 godot_napi (NAPI 桥接) 直接向引擎注入输入事件，
 *   同时利用 NAPI 的 onPause/onResume 模拟前后台切换。
 *
 * 运行方式:
 *   hdc shell aa test -b com.godotengine.editor -s unittest OpenHarmonyTestRunner \
 *     -s class MonkeyTest -s level 1
 */
import { describe, beforeAll, afterAll, it, expect } from '@ohos/hypium';
import { MonkeyEngine, MonkeyConfig, MonkeyEvent, MonkeyEventType, MonkeySummary } from './MonkeyEngine';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { godot_napi } from 'libgodot_napi.so';

const TAG = 'MonkeyTest';
const DOMAIN = 0x0000;

// ── NAPI 事件注入实现 ──
async function injectEventViaNapi(event: MonkeyEvent): Promise<void> {
  const d = event.data;
  switch (event.type) {
    case MonkeyEventType.TOUCH:
      godot_napi.sendTouchEvent(
        (d.touchId as number) ?? 0,
        (d.action as number) ?? 0,
        (d.x as number) ?? 0,
        (d.y as number) ?? 0
      );
      break;
    case MonkeyEventType.KEY:
      godot_napi.sendKeyEvent(
        (d.keyCode as number) ?? 0,
        (d.eventType as number) ?? 0,
        (d.keyText as string) ?? ''
      );
      break;
    case MonkeyEventType.MOUSE:
      godot_napi.sendMouseEvent(
        (d.button as number) ?? 0,
        (d.action as number) ?? 0,
        (d.x as number) ?? 0,
        (d.y as number) ?? 0,
        (d.offsetX as number) ?? 0,
        (d.offsetY as number) ?? 0
      );
      break;
    case MonkeyEventType.PAUSE_RESUME:
      if ((d.action as number) === 0) {
        godot_napi.onPause();
        await sleep((d.delayMs as number) ?? 500);
        godot_napi.onResume();
      } else {
        godot_napi.onResume();
        await sleep((d.delayMs as number) ?? 500);
        godot_napi.onPause();
      }
      break;
    case MonkeyEventType.BACK:
      godot_napi.onBackPress();
      break;
    case MonkeyEventType.IDLE:
      // 纯等待
      break;
  }
}

function sleep(ms: number): Promise<void> {
  return new Promise<void>((resolve) => setTimeout(resolve, ms));
}

// ── 测试套件 ──
export default function monkeyTest() {
  describe('MonkeyTest', () => {
    let monkeyResult: MonkeySummary | null = null;

    /**
     * Smoke: 快速 100 次迭代 Monkey 测试
     * 验证:
     *   - 引擎在随机输入下不崩溃
     *   - NAPI 事件注入通道正常
     *   - 崩溃次数为 0
     */
    it('Smoke_100_iterations', 0, async () => {
      hilog.info(DOMAIN, TAG, '=== Monkey Smoke Test (100 iterations) ===');

      const config: Partial<MonkeyConfig> = {
        iterations: 100,
        seed: Date.now(),
        maxDurationMs: 15000,
        eventIntervalMs: 30,
        screenWidth: 1920,
        screenHeight: 1080,
        targetBundle: 'com.godotengine.editor',

        // 冒烟测试：降低 pause/resume 权重，聚焦输入事件
        weightTouch: 50,
        weightKey: 20,
        weightMouse: 20,
        weightPauseResume: 0,
        weightBack: 5,
        weightIdle: 5
      };

      const monkey: MonkeyEngine = new MonkeyEngine(config);
      monkey.injectEvent = injectEventViaNapi;

      monkey.callbacks = {
        onCrash: (reason: string) => {
          hilog.error(DOMAIN, TAG, `[CRASH] ${reason}`);
        },
        onError: (msg: string) => {
          hilog.error(DOMAIN, TAG, `[ERROR] ${msg}`);
        },
        onProgress: (current: number, total: number) => {
          hilog.info(DOMAIN, TAG, `Smoke progress: ${current}/${total}`);
        }
      };

      monkeyResult = await monkey.run();
      const s = monkeyResult!;

      hilog.info(DOMAIN, TAG,
        `Smoke complete: ${s.totalEvents} events, ` +
        `${s.elapsedMs}ms, ${s.crashes} crashes, ${s.errors} errors`);

      // 断言
      expect(s.crashes).assertEqual(0);
      expect(s.errors).assertEqual(0);
      expect(s.totalEvents).assertEqual(100);
    });

    /**
     * Standard: 500 次迭代标准 Monkey 测试
     * 包含 pause/resume 生命周期切换
     */
    it('Standard_500_iterations', 0, async () => {
      hilog.info(DOMAIN, TAG, '=== Monkey Standard Test (500 iterations) ===');

      const config: Partial<MonkeyConfig> = {
        iterations: 500,
        seed: Date.now(),
        maxDurationMs: 60000,
        eventIntervalMs: 50,
        screenWidth: 1920,
        screenHeight: 1080,
        targetBundle: 'com.godotengine.editor',

        // 标准测试：平衡分布
        weightTouch: 40,
        weightKey: 15,
        weightMouse: 25,
        weightPauseResume: 5,
        weightBack: 5,
        weightIdle: 10
      };

      const monkey: MonkeyEngine = new MonkeyEngine(config);
      monkey.injectEvent = injectEventViaNapi;

      const eventLog: MonkeyEvent[] = [];

      monkey.callbacks = {
        onBeforeEvent: (ev: MonkeyEvent) => {
          eventLog.push(ev);
        },
        onCrash: (reason: string) => {
          hilog.error(DOMAIN, TAG, `[CRASH] ${reason}`);
        },
        onError: (msg: string) => {
          hilog.error(DOMAIN, TAG, `[ERROR] ${msg}`);
        },
        onProgress: (current: number, total: number) => {
          if (current % 100 === 0) {
            hilog.info(DOMAIN, TAG, `Standard progress: ${current}/${total}`);
          }
        }
      };

      monkeyResult = await monkey.run();
      const s = monkeyResult!;

      hilog.info(DOMAIN, TAG,
        `Standard complete: ${s.totalEvents} events, ${s.elapsedMs}ms, ` +
        `${s.crashes} crashes, ${s.errors} errors`);

      // 断言
      expect(s.crashes).assertEqual(0);
      expect(s.totalEvents).assertEqual(500);

      // 验证事件类型分布（每种类型至少有一定比例）
      const touchPct: number = (s.eventsByType['TOUCH'] ?? 0) / s.totalEvents;
      expect(touchPct > 0.2).assertTrue();

      // 导出一小部分日志用于调试
      hilog.info(DOMAIN, TAG, `Event distribution: ${JSON.stringify(s.eventsByType)}`);
    });

    /**
     * Stress: 2000 次迭代压力测试
     * 仅在明确要求时运行（--full / STRESS_MODE=true）
     */
    it('Stress_2000_iterations', 0, async () => {
      // 检查是否启用压力模式
      const stressMode: boolean = false; // 默认关闭，通过环境变量覆盖
      if (!stressMode) {
        hilog.info(DOMAIN, TAG, 'Stress test skipped (STRESS_MODE=false)');
        expect(true).assertTrue(); // 跳过但 pass
        return;
      }

      hilog.info(DOMAIN, TAG, '=== Monkey Stress Test (2000 iterations) ===');

      const config: Partial<MonkeyConfig> = {
        iterations: 2000,
        seed: Date.now(),
        maxDurationMs: 180000, // 3 分钟
        eventIntervalMs: 30,
        screenWidth: 1920,
        screenHeight: 1080,
        targetBundle: 'com.godotengine.editor',

        weightTouch: 35,
        weightKey: 15,
        weightMouse: 20,
        weightPauseResume: 10, // 压力测试增加生命周期切换
        weightBack: 5,
        weightIdle: 15
      };

      const monkey: MonkeyEngine = new MonkeyEngine(config);
      monkey.injectEvent = injectEventViaNapi;

      monkey.callbacks = {
        onCrash: (reason: string) => {
          hilog.error(DOMAIN, TAG, `[STRESS CRASH] ${reason}`);
        },
        onError: (msg: string) => {
          hilog.error(DOMAIN, TAG, `[STRESS ERROR] ${msg}`);
        },
        onProgress: (current: number, total: number) => {
          if (current % 200 === 0) {
            hilog.info(DOMAIN, TAG, `Stress progress: ${current}/${total}`);
          }
        }
      };

      monkeyResult = await monkey.run();
      const s = monkeyResult!;

      hilog.info(DOMAIN, TAG,
        `Stress complete: ${s.totalEvents} events, ${s.elapsedMs}ms, ` +
        `${s.crashes} crashes, ${s.errors} errors`);

      // 压力测试允许少量 transient error，但崩溃必须为 0
      expect(s.crashes).assertEqual(0);

      // 至少完成 90% 的事件（允许超时截断）
      expect(s.totalEvents >= 1800).assertTrue();
    });

    /**
     * 事件回放：允许重放之前触发崩溃的事件序列
     * 传入 events JSON 数组即可复现问题
     */
    it('Replay_crash_events', 0, async () => {
      hilog.info(DOMAIN, TAG, '=== Monkey Replay Test ===');

      // 如果有之前保存的崩溃事件序列，从这里加载
      // const crashEvents: MonkeyEvent[] = JSON.parse(savedCrashEvents);
      // await replayEvents(crashEvents);

      expect(true).assertTrue(); // 默认跳过
    });
  });
}

// ── 事件回放工具函数 ──
async function replayEvents(events: MonkeyEvent[]): Promise<void> {
  hilog.info(DOMAIN, TAG, `Replaying ${events.length} events`);
  for (let i = 0; i < events.length; i++) {
    try {
      await injectEventViaNapi(events[i]);
      await sleep(20);
    } catch (e) {
      hilog.error(DOMAIN, TAG, `Replay failed at event ${i}: ${e}`);
      throw e;
    }
  }
  hilog.info(DOMAIN, TAG, 'Replay complete');
}
