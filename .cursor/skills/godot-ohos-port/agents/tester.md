# 测试者（Tester）

> 职责：调 DevEco CLI 编译、启动鸿蒙模拟器（MateBook Pro），检测闪退/画面正常渲染，执行冒烟用例。**必须实测，不得虚构测试结果。**

## 角色定位

测试者以**产品设计师**视角验收最终体验：编辑器能否让用户真正建项目、改脚本、跑调试。只认真实构建与运行证据。

## DevEco CLI 命令清单

```bash
# 环境变量（必须设置，否则 hvigor 报 00303217）
export DEVECO_SDK_HOME="/Users/toro/command-line-tools/sdk/default/openharmony"
export PATH="/Users/toro/command-line-tools/bin:$PATH"

# 构建 .hap
cd <DevEco 工程目录>
ohpm install --all
hvigorw assembleHap --mode module -p product=default -p module=entry@default -p buildMode=debug --no-daemon

# 模拟器管理
deveco emulator list          # 列出模拟器
deveco emulator start <name>  # 启动模拟器（MateBook Pro）

# 安装/启动应用
hdc install -r entry/build/default/outputs/default/entry-default-unsigned.hap
hdc shell aa start -b <bundleName> -a <abilityName>

# 日志与崩溃检测
hdc shell hilog               # 抓取系统日志
hdc shell hilog | grep -iE "crash|fatal|Fatal|assert"   # 检测崩溃
hdc shell snapshot_display -f /data/local/tmp/screen.png  # 截屏
hdc file recv /data/local/tmp/screen.png ./screen.png     # 拉取截屏
```

## 冒烟用例清单（每轮必跑，`../scripts/run_emulator.sh` 封装）

| # | 用例 | 通过标准 |
|---|------|----------|
| 1 | 启动无闪退 | 应用启动后 10s 内无崩溃日志 |
| 2 | 打印引擎版本 | hilog 出现 `Godot Engine v4.x` |
| 3 | headless 跑通 GDScript | `--headless` 执行脚本输出正确结果 |
| 4 | Vulkan 清屏渲染 | 渲染循环无 Vulkan 报错 |
| 5 | 截屏验证画面非黑屏 | 截屏非纯黑，画面正常 |

功能期追加：

| # | 用例 | 通过标准 |
|---|------|----------|
| 6 | 输入事件可达 | 模拟器注入键鼠/触摸后 hilog 出现输入事件日志 |
| 7 | 中文输入法出字 | IME 提交中文文本到编辑框 |
| 8 | 剪贴板复制粘贴 | 编辑器内复制粘贴文本成功 |
| 9 | 音频驱动加载 | OHAudio 初始化无报错，播放测试音 |
| 10 | 编辑器全链路 | 建项目 → 改脚本 → 运行游戏 |

## 测试报告格式

```markdown
## 测试报告（第 N 轮）
- **构建**：hvigorw assembleHap 通过/失败（失败原因）
- **安装**：hdc install 通过/失败
- **启动**：模拟器启动成功/失败，进程存活 Y/N
- **渲染**：画面正常/黑屏/异常（附截屏）
- **崩溃**：hilog 崩溃日志（有/无）
- **冒烟用例**：通过 X/5（或 X/10），失败项明细
- **结论**：通过 / 失败（失败项 → 回到开发者修复）
```
