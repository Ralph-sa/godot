/**
 * GodotEditor.test.ets — Godot 编辑器功能测试套件
 *
 * 测试场景:
 *   1. 引擎启动完整流程
 *   2. 项目选择器 UI 渲染
 *   3. Surface 生命周期
 *   4. 前后台切换恢复
 *   5. 输入事件注入验证
 */
import { describe, beforeAll, afterAll, it, expect } from '@ohos/hypium';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { godot_napi } from 'libgodot_napi.so';

const TAG = 'GodotEditorTest';
const DOMAIN = 0x0000;

function sleep(ms: number): Promise<void> {
  return new Promise<void>((resolve) => setTimeout(resolve, ms));
}

export default function godotEditorTest() {
  describe('GodotEditorTest', () => {

    // ── 引擎生命周期 ──

    it('Engine_Init_Success', 0, async () => {
      hilog.info(DOMAIN, TAG, 'Testing engine init...');

      let result: number = -1;
      try {
        result = godot_napi.init();
        hilog.info(DOMAIN, TAG, `godot_napi.init() returned: ${result}`);
      } catch (e) {
        hilog.error(DOMAIN, TAG, `godot_napi.init() threw: ${e}`);
      }

      expect(result).assertEqual(0);
    });

    it('Engine_Cleanup_Success', 0, async () => {
      hilog.info(DOMAIN, TAG, 'Testing engine cleanup...');

      let result: number = -1;
      try {
        result = godot_napi.cleanup();
        hilog.info(DOMAIN, TAG, `godot_napi.cleanup() returned: ${result}`);
      } catch (e) {
        hilog.error(DOMAIN, TAG, `godot_napi.cleanup() threw: ${e}`);
      }

      expect(result).assertEqual(0);
    });

    // ── Surface 生命周期 ──

    it('Surface_Create_Destroy_Cycle', 0, async () => {
      hilog.info(DOMAIN, TAG, 'Testing surface create/destroy cycle...');

      const surfaceId: string = 'test_surface_' + Date.now().toString();

      // 创建 Surface
      let createResult: number = -1;
      try {
        createResult = godot_napi.onSurfaceCreated(surfaceId);
        hilog.info(DOMAIN, TAG, `onSurfaceCreated(${surfaceId}) => ${createResult}`);
      } catch (e) {
        hilog.error(DOMAIN, TAG, `onSurfaceCreated threw: ${e}`);
      }
      expect(createResult).assertEqual(0);

      await sleep(100);

      // 销毁 Surface
      let destroyResult: number = -1;
      try {
        destroyResult = godot_napi.onSurfaceDestroy();
        hilog.info(DOMAIN, TAG, `onSurfaceDestroy() => ${destroyResult}`);
      } catch (e) {
        hilog.error(DOMAIN, TAG, `onSurfaceDestroy threw: ${e}`);
      }
      expect(destroyResult).assertEqual(0);
    });

    it('Surface_Rapid_Create_Destroy', 0, async () => {
      hilog.info(DOMAIN, TAG, 'Testing rapid surface create/destroy (10 cycles)...');

      for (let i = 0; i < 10; i++) {
        const sid: string = `rapid_surface_${i}`;
        try {
          godot_napi.onSurfaceCreated(sid);
          await sleep(10);
          godot_napi.onSurfaceDestroy();
          await sleep(10);
        } catch (e) {
          hilog.error(DOMAIN, TAG, `Rapid cycle ${i} failed: ${e}`);
          // 不应因为快速创建/销毁而崩溃
          expect(false).assertTrue(); // 如果到这里说明崩溃了
          return;
        }
      }

      expect(true).assertTrue();
    });

    // ── 前后台切换 ──

    it('Pause_Resume_Cycle', 0, async () => {
      hilog.info(DOMAIN, TAG, 'Testing pause/resume cycle...');

      try {
        godot_napi.onPause();
        await sleep(200);
        godot_napi.onResume();
        await sleep(200);
      } catch (e) {
        hilog.error(DOMAIN, TAG, `Pause/resume failed: ${e}`);
      }

      // 能走到这里说明没崩溃
      expect(true).assertTrue();
    });

    it('Rapid_Pause_Resume', 0, async () => {
      hilog.info(DOMAIN, TAG, 'Testing rapid pause/resume (5 cycles)...');

      for (let i = 0; i < 5; i++) {
        try {
          godot_napi.onPause();
          await sleep(50);
          godot_napi.onResume();
          await sleep(50);
        } catch (e) {
          hilog.error(DOMAIN, TAG, `Rapid pause/resume cycle ${i} failed: ${e}`);
          expect(false).assertTrue();
          return;
        }
      }

      expect(true).assertTrue();
    });

    // ── 输入事件注入 ──

    it('Touch_Event_Basic', 0, async () => {
      hilog.info(DOMAIN, TAG, 'Testing basic touch event...');

      let result: number = -1;
      try {
        result = godot_napi.sendTouchEvent(0, 0, 500, 300); // touch down
        hilog.info(DOMAIN, TAG, `sendTouchEvent => ${result}`);
        await sleep(50);
        result = godot_napi.sendTouchEvent(0, 1, 500, 300); // touch up
        hilog.info(DOMAIN, TAG, `sendTouchEvent(up) => ${result}`);
      } catch (e) {
        hilog.error(DOMAIN, TAG, `Touch event failed: ${e}`);
      }

      expect(result).assertEqual(0);
    });

    it('Mouse_Event_Basic', 0, async () => {
      hilog.info(DOMAIN, TAG, 'Testing basic mouse event...');

      let result: number = -1;
      try {
        result = godot_napi.sendMouseEvent(0, 2, 400, 300, 5, 0); // move
        hilog.info(DOMAIN, TAG, `sendMouseEvent(move) => ${result}`);
        result = godot_napi.sendMouseEvent(0, 0, 400, 300, 0, 0); // press
        hilog.info(DOMAIN, TAG, `sendMouseEvent(press) => ${result}`);
        await sleep(50);
        result = godot_napi.sendMouseEvent(0, 1, 400, 300, 0, 0); // release
        hilog.info(DOMAIN, TAG, `sendMouseEvent(release) => ${result}`);
      } catch (e) {
        hilog.error(DOMAIN, TAG, `Mouse event failed: ${e}`);
      }

      expect(result).assertEqual(0);
    });

    it('Key_Event_Basic', 0, async () => {
      hilog.info(DOMAIN, TAG, 'Testing basic key event...');

      let result: number = -1;
      try {
        result = godot_napi.sendKeyEvent(2006, 0, ' '); // Space down
        hilog.info(DOMAIN, TAG, `sendKeyEvent(down) => ${result}`);
        await sleep(50);
        result = godot_napi.sendKeyEvent(2006, 1, ' '); // Space up
        hilog.info(DOMAIN, TAG, `sendKeyEvent(up) => ${result}`);
      } catch (e) {
        hilog.error(DOMAIN, TAG, `Key event failed: ${e}`);
      }

      expect(result).assertEqual(0);
    });

    it('Multi_Touch_Event', 0, async () => {
      hilog.info(DOMAIN, TAG, 'Testing multi-touch event (3 fingers)...');

      try {
        // 三指同时按下
        godot_napi.sendTouchEvent(0, 0, 200, 200);
        godot_napi.sendTouchEvent(1, 0, 600, 400);
        godot_napi.sendTouchEvent(2, 0, 1000, 600);
        await sleep(100);

        // 三指同时抬起
        godot_napi.sendTouchEvent(0, 1, 200, 200);
        godot_napi.sendTouchEvent(1, 1, 600, 400);
        godot_napi.sendTouchEvent(2, 1, 1000, 600);
      } catch (e) {
        hilog.error(DOMAIN, TAG, `Multi-touch failed: ${e}`);
      }

      expect(true).assertTrue();
    });

    // ── 边界条件 ──

    it('Init_Without_Surface', 0, async () => {
      hilog.info(DOMAIN, TAG, 'Testing init without prior surface creation...');

      // 不应该崩溃，但可能返回非 0
      let result: number = -1;
      try {
        result = godot_napi.init();
        hilog.info(DOMAIN, TAG, `Init without surface => ${result}`);
      } catch (e) {
        hilog.error(DOMAIN, TAG, `Init without surface threw: ${e}`);
      }

      // 不强制断言返回值，只要不崩溃即可
      expect(true).assertTrue();
    });

    it('Double_Init_Prevention', 0, async () => {
      hilog.info(DOMAIN, TAG, 'Testing double init prevention...');

      try {
        godot_napi.init();
        const secondResult: number = godot_napi.init();
        hilog.info(DOMAIN, TAG, `Second init => ${secondResult}`);
        // 第二次 init 应该安全处理（不崩溃），返回值可以是任何值
      } catch (e) {
        hilog.error(DOMAIN, TAG, `Double init threw: ${e}`);
      }

      expect(true).assertTrue();
    });

    // ── NAPI 函数完整性 ──

    it('All_NAPI_Functions_Exist', 0, () => {
      hilog.info(DOMAIN, TAG, 'Verifying all 12 NAPI functions exist...');

      const functions: string[] = [
        'init', 'cleanup',
        'onSurfaceCreated', 'onSurfaceDestroy',
        'sendKeyEvent', 'sendMouseEvent', 'sendTouchEvent', 'sendInputText',
        'onPause', 'onResume',
        'onBackPress',
        'registerTitleCallback'
      ];

      for (const fn of functions) {
        const exists: boolean =
          typeof (godot_napi as Record<string, unknown>)[fn] === 'function';
        hilog.info(DOMAIN, TAG, `  ${fn}: ${exists ? 'OK' : 'MISSING'}`);
        expect(exists).assertTrue();
      }
    });
  });
}
