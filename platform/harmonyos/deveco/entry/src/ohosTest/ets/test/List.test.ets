/**
 * List.test.ets — hypium 框架基准测试
 *
 * 每个 hypium 测试工程至少需要一个测试套件作为入口。
 * 此文件包含最基本的单元测试，验证测试框架本身正常工作。
 */
import { describe, beforeAll, afterAll, it, expect } from '@ohos/hypium';

export default function listTest() {
  describe('BaselineTest', () => {

    beforeAll(() => {
      console.info('BaselineTest beforeAll: framework is ready');
    });

    afterAll(() => {
      console.info('BaselineTest afterAll: all tests complete');
    });

    it('Framework_Boot_Check', 0, () => {
      // 验证 hypium 框架本身正常运行
      expect(1 + 1).assertEqual(2);
      expect('hypium').assertContain('hyp');
    });

    it('string_assertions', 0, () => {
      const appName: string = 'Godot Editor';
      expect(appName).assertEqual('Godot Editor');
      expect(appName.length).assertEqual(12);
    });

    it('number_assertions', 0, () => {
      const sdkVersion: number = 24;
      expect(sdkVersion).assertEqual(24);
      expect(sdkVersion > 0).assertTrue();
      expect(-1 < 0).assertTrue();
    });

    it('boolean_assertions', 0, () => {
      expect(true).assertTrue();
      expect(false).assertFalse();
      expect(1 === 1).assertTrue();
    });

    it('null_undefined_checks', 0, () => {
      const nullVal: null = null;
      const undefVal: undefined = undefined;
      expect(nullVal).assertNull();
      expect(undefVal).assertUndefined();
    });
  });
}
