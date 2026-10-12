# V0.3.0 · 首个公开版本：贴壁纸层的下班倒计时 + 纪念日桌面组件

## 版本信息

| 项 | 内容 |
| --- | --- |
| 版本号 | V0.3.0 |
| 发布日期 | 2026-10-12 |
| 上一版本 | 无（首个公开版本） |
| 变更类型 | 首个公开版本（minor：新系统 + 新页面，非 patch） |
| 影响文件 | 全部（首次入库）。主要模块：`src/`（`DesktopLayer` 桌面层 / `ConfigStore` 配置 v2 / `CardBackend` 卡片数据 / `WidgetManager` 多卡片 / `Festivals` 节日与农历 / `SettingsBackend`+`SettingsWindow` 设置面板 / `TrayController` 托盘 / `AutoStart` 开机自启 / `main.cpp`）、`qml/`（`Main` `CardSlot` `CountdownCard` `AnniversaryCard` `Mascot` `SettingsWindow`）、`assets/`（`tray.ico` `mascot.png` `festivals.json`）、`tools/`（构建/打包/回归/素材处理/隐私扫描）、`docs/`（渲染样张） |
| 未改动文件 | 无历史基线可比。结构性证据：`qml/CountdownCard.qml` 的窄版 320×150 / 宽版 470×206 设计稿尺寸与字号自第 5 轮定稿后未再改动，本版只在其空档处新增形象位；运行期依赖集未变（**不引入任何第三方运行时库**，农历与节日数据全部由 `tools/gen_festivals.py` 离线预生成进 `assets/festivals.json`） |
| 兼容性 | 配置文件为 v2（`cards[]` 数组）。若检测到旧的 v1 扁平配置会自动迁移为 `cards[0]`，位置与设置不丢。程序数据目录固定为 `%APPDATA%\wei\DesktopWidget\` |
| 运行要求 | Windows 10/11 x64。绿色包自带 Qt 6.8.3 运行库与 VC143 CRT，无需安装 Qt / VC++ 运行库。源码构建需要 Qt 6.8 msvc2022_64 + VS2022(BuildTools) |

## 更新与修改

### 1. 贴壁纸层的窗口（本项目的技术核心）

- 无边框 / 透明 / 不进任务栏与 Alt-Tab 的窗口，用 `SetParent` 挂到桌面层。
- **挂载前必须先把窗口样式改成 `WS_CHILD` 并清掉 `WS_POPUP`**，否则挂载后不出帧。
- 目标层自动判定：Windows 11 25H2 上是 **`Progman`**（用 `0x052C` 消息造出的 `WorkerW` 在本机是 202×56 的隐身窗口，不能挂）；老 Win10 是 `WorkerW`。`DW_LAYER` 可强制指定，另有 3 秒看门狗自动重挂（explorer 重启后自愈）。
- **z 序**：插到 `SHELLDLL_DefView` 正下方（图标之下、壁纸之上）。**禁用 `HWND_BOTTOM`** —— 那会把卡片沉到壁纸层 `WorkerW` 之下被盖住，表现为"卡片凭空消失"。
- 换父窗口后必须 `forceRepaint` + hide/show 强制 Qt 出帧。

### 2. 两张卡片 / 多卡片架构

- **一张卡 = 一个 `CardBackend` + 一个 `QQuickView`**，各自挂桌面层、各自摆位与配色。不动已验证的 Win32 挂载逻辑，只是按窗口循环一遍。
- 倒计时卡：按工作时段实时走秒；四个小格 = 距发薪日 / 距周五 / 下一个节日 / 今天赚了（按秒累积，3 位小数）。
- 纪念日卡：小字事件名 + 大号天数 + 底部日期，背景支持 纯色 / 内置月夜 / 自定义图片。
- 位置三重钳制（拖动中、存盘前、启动还原时）到显示器工作区；完全在屏外自动拉回。

### 3. 纪念日三种模式

| 模式 | 口径 | 循环规则 |
| --- | --- | --- |
| 倒计时（还有多少天） | 目标日 − 今天 | 不重复 / 每年 / 每月（2/29 遇平年退到 2/28） |
| 纪念日（已经多少天） | 今天 − 起始日 | 一直累加（隐藏循环选择） |
| **农历节日**（新增） | 按农历算下一个节日 | 春节/元宵/龙抬头/端午/七夕/中元/中秋/重阳/腊八/小年/**除夕**；或自定义农历月/日（农历生日） |

口径只写一份：`namespace AnnivMath`，卡片计算与设置界面"卡片会显示：…"预览**共用同一个函数**。

### 4. 设置面板

- 左侧实时预览 + 右侧深色配置行；**改动即时生效，没有保存按钮**。
- 顶部卡片圆点选择器 + 「＋倒计时 / ＋纪念日 / 删除」。
- 按选中卡片类型显示配置行（倒计时卡有工作时段/发薪日/四宫格开关等，纪念日卡有模式/事件名/日期或农历节日/背景）。
- 自绘深色**日历选择器**、HSV 取色器。

### 5. 节日与农历数据（离线预生成）

- `tools/gen_festivals.py` 用 `cnlunar` 生成 2025–2045：482 条节日（含传统节日与清明/冬至节气）+ 193 天放假日 + 11 天调休补班 + **260 条农历月初表**（任意农历月/日 → 公历，含闰月）。
- 官方放假/调休取自 `NateScarlet/holiday-cn`（当前已公布 2025、2026）。**未公布年份退回"法定假日照休 + 用户自配工作日"**，不凭空编调休。
- 程序运行时不联网、不引农历库。

### 6. 卡通形象 / 托盘 / 开机自启 / 打包

- 形象默认用用户提供的猫图（`assets/mascot.png`），设置里可选「图片 / 矢量自绘 / 隐藏」。
- 托盘菜单：显示与隐藏、编辑模式、重新挂到桌面层、重置位置、新增卡片、设置、开机自启、退出（带二次确认）。每个动作写日志，避免"组件凭空消失"却无从诊断。
- 开机自启写 HKCU 的 Run 键（无需管理员）；绿色包被移动目录后启动时会自动纠正注册表里的旧路径。
- `tools/package.bat` 用 `windeployqt` 部署 Qt 运行库 + 平台插件 + QML 模块，并自包含 VC143 CRT（并删掉 windeployqt 附带的多余 `vc_redist.x64.exe`，省 25MB）。

## 解决的问题

| # | 问题 | 措施 | 生效位置 |
| --- | --- | --- | --- |
| 1 | **缩小后字不清**：312px 宽（用户实际尺寸）下数字发虚 | 实测定位真因是**文字渲染**而非分辨率（Laplacian：Qt 默认 1292 / `Text.NativeRendering` 2775，+115%）；全部文字改 `Text.NativeRendering`，弃用灰度描边/软阴影/`PreferFullHinting` | `qml/CountdownCard.qml`、`qml/AnniversaryCard.qml` |
| 2 | **拖动卡片、点"完成"后卡片消失** | 真因是退出编辑重挂回 `Progman` 时用了 `HWND_BOTTOM`，卡片被沉到壁纸层之下；改为插到 `SHELLDLL_DefView` 正下方，并清 `WS_EX_TOPMOST` + `forceRepaint` + hide/show | `src/DesktopLayer.cpp` |
| 3 | **"设置日期没反应"**（用户报告）：输入框里留着自己打的字、模型还是旧值、卡片毫无变化 | 两条根因：① `inputMask:"9999-99-99"` 会放行 `2027-3-5`（解析失败）又认为 `2027-02-31` 完全合法（`acceptableInput=true`）；② 解析失败时 QML 的"回退显示"被 `!field.activeFocus` 挡住（按回车时焦点仍在框里，回退永不执行）。改为去掉掩码 + 宽松解析（`2027-3-5`/`2027/03/05`/`2027.3.5`/`2027年3月5日`/`20270305`）+ `Q_INVOKABLE bool applyDateInput()` 明确返回成功/失败 + 失败即回退并弹提示 | `src/SettingsBackend.cpp`、`qml/SettingsWindow.qml` |
| 4 | **农历模式整个不生效**：切到"农历节日"后设置界面那一行不出现、预览数字还是旧的 | 设置侧 `annivMode()` 沿用了老的两值逻辑（"非 countdown 即 anniversary"），把 `lunar` 吃掉了。改为三种模式原样透传 | `src/SettingsBackend.cpp` |
| 5 | 农历月/日下拉**选了相同值不切到"自定义"** | 早返回导致不清状态；改成"数值没变也要切到 custom 并通知" | `src/SettingsBackend.cpp` |
| 6 | 农历那两个小下拉框**显示成空框** | 窄框里箭头 + 内边距把文本区挤成 0 宽；按宽度收敛内边距并加宽到 92px | `qml/SettingsWindow.qml` |
| 7 | 卡片图片背景**四角不圆** | `Rectangle{radius}` 不裁子项，`Canvas` 的 `clip()` 对 `drawImage` 无效（对 `fill` 有效，容易误判）；改用 `destination-out` 反向擦掉四角 | `qml/AnniversaryCard.qml` |
| 8 | 素材是**JPG + 画上去的棋盘格**（JPEG 无 alpha 通道），抠图后残留斑点、裁剪框被撑满整张画布 | 从四边泛洪抠背景 + 阈值放宽到 228 吃下压缩噪点 + **只保留最大连通块**丢掉碎块 | `tools/prepare_mascot.py` |
| 9 | `.bat` 里写中文导致 cmd 报 `'?setlocal' 不是内部或外部命令` | cmd 按 GBK 读 .bat，中文尾字节吃掉引号/续行；**所有 .bat 保持纯 ASCII**，中文提示放 README | `tools/*.bat` |
| 10 | 打包脚本报"目录正被占用"而失败 | `dist` 目录恰好是某个 shell 的当前目录，`rmdir` 失败；改为清空内容而非删目录 | `tools/package.bat` |
| 11 | 多窗口下第 2 张卡退出编辑模式时被还原成第 1 张卡的窗口样式 | 桌面层保存"原始窗口样式"的结构原本是全局单份；改为按 HWND 的 `QHash` | `src/DesktopLayer.cpp` |
| 12 | 误判澄清：曾以为 `MultiEffect{maskEnabled; maskSource}` 做圆角遮罩"静默失效" | 实为**视觉误读**：透明区域在视图里被合成成背景色，而圆角附近残留的图片内容看起来像"角没裁掉"。实测两种做法像素仅差 0.3%（只差抗锯齿边）。**结论：渲染效果一律用像素验收，不信文字描述** | 记录于 `docs/mascot-compare-src.png` 同批诊断 |
| 13 | 误判澄清：生成节日表的脚本"卡死" | `cnlunar` 是逐日算农历的，21 年 + 月初表约 7 万次调用，实测约 11 分钟。已在脚本 docstring 写明耗时 | `tools/gen_festivals.py` |

## 实现效果

全部结论都有命令与数字：

```
# 构建
MSYS_NO_PATHCONV=1 cmd /c "tools\build.bat"
  → [BUILD OK] D:\desktop-widget\build\DesktopWidget.exe

# 卡片挂载（启动日志）
[card] 0 下班倒计时 … rect=(2084,829 468x219) parent=Progman 0x1013c … selfVisible=是 topmost=否
[card] 1 纪念日   … rect=(2084,1069 468x219) parent=Progman 0x1013c … selfVisible=是 topmost=否
[festivals] 已载入 482 个节日，放假日 193 天，调休补班 11 天，官方年份 QList(2025, 2026)
            农历月初 260 条/农历节日 11 个

# 口径自检（DW_FREEZE=2026-10-12 钉死时间，完全可复现）
DW_TEST_ANNIV=1  → 口径用例 6/6 通过
    anniversary/2003-11-25/none → 8357 天 ✓（与参考截图一致）
    countdown/2027-03-15/none   → 154 天 ✓
    countdown/2000-05-20/yearly → 220 天 ✓
    countdown/2000-10-20/monthly→ 8 天 ✓
    输入 2027-3-5 → 接受并归一化为 2027-03-05；2027-02-31 / abc / 空 → 拒绝且配置不变
DW_TEST_MODE=1   → 倒计时/2027-3-5 = 144 天；过去日期 = 0 天 + 红字提醒；每年 = 44 天；纪念日 = 8357 天
DW_TEST_LUNAR=1  → 用例 9/9 通过
    春节→2027-02-06、除夕→2027-02-05、中秋→2027-09-15、重阳→2026-10-18、端午→2027-06-09
    农历九月初九→2026-10-18、腊月廿三→2027-01-30、腊月三十→2027-02-05（该年腊月只有 29 天，退到月末）
DW_DEL_CARD=1    → 3 张删中间那张 → 剩余 2 张且索引正确重排
DW_AUTOSTART=1   → 写注册表 → 读回校验 true → 复原（注册表值已清空）

# 清晰度（Laplacian 方差，越大越锐）
屏上旧版 832.3 / 385.8  →  离屏 Qt 默认渲染 1292 / NativeRendering 2775（+115%）/ 312px 宽 3192（+69%）

# 回归脚本
bash tools/verify_ui.sh   → 12 节全绿（编辑模式端到端、位置持久化、越界自愈、5 种层策略、
                            退出编辑后仍可见、设置即时生效、新增/删除卡片、开机自启、日期解析、
                            两种模式、农历换算）

# 绿色包自包含（把 PATH 清到只剩系统目录再跑）
env -i SystemRoot='C:\Windows' PATH='C:\Windows\System32;C:\Windows' ./DesktopWidget.exe
  → exit 0，两张卡片正常渲染（self-shot 474×219 各一张）
包大小：150 个文件 / 压缩后约 29 MB
```

截图与样张见 `docs/`：`final-desktop-user-cat.png`（桌面实拍）、`card-mascot-narrow.png`、`settings-anniv-lunar.png`、`card-lunar.png`、`mascot-compare-src.png`、`sharpness-compare.png`。

## 待解决问题

| # | 问题 | 影响 | 下一步 |
| --- | --- | --- | --- |
| 1 | `assets/mascot.png` 是使用者提供的第三方美术素材 | **不在 MIT 许可范围内**；若要再分发请自行确认授权 | 需要使用者决定：换成自制素材或保留（本仓库按"作者自用"保留） |
| 2 | 卡片吸附对齐未做（拖动时贴边、卡片之间互相对齐） | 手工摆位可能差几像素 | 在 `WidgetManager` 的拖动逻辑里加吸附阈值 |
| 3 | 多显示器：按卡片所在显示器钳制，但**跨屏摆放未实测** | 多屏用户行为未知 | 需要多屏环境实测 |
| 4 | 生日等**农历表只覆盖 2025–2045** | 超出范围卡片显示 `-` 并给提示 | 需要时重跑 `tools/gen_festivals.py` 扩范围（约 11 分钟） |
| 5 | 没有单实例锁 | 手动双击两次会出现两份卡片 | 加 `QSharedMemory` 或命名互斥体 |
| 6 | 无 CI：回归验证依赖真实 Windows 桌面 + 本机 Qt/MSVC（挂载 Progman、编辑模式都需要真桌面） | 他人 PR 无法自动验证 | 可先做"仅构建"的 Actions（windows-latest + aqtinstall），桌面相关仍标注"仅本地执行" |
| 7 | 内存：每张卡约 +20 MB（一张卡一个 QQuickView） | 卡片多时占用线性增长 | 若需要可评估合并为单窗口多 item |
| 8 | 只在 **Win11 25H2 / 单屏 2560×1600 @150%** 上实测 | 其他 DPI、分辨率、Win10 未验证 | 需要更多环境实测 |
| 9 | 未公布年份的放假口径是"法定假日照休 + 用户自配工作日" | 那些年份的调休与实际可能不同 | 每年官方公布后重跑生成脚本 |

## 验证记录

```
python tools/privacy_scan.py .        → 命中 0 处（推送公开仓库前）
python -m py_compile tools/*.py       → 全部语法 OK
bash -n tools/verify_ui.sh            → 语法 OK
检查 tools/*.bat 非 ASCII 字节         → build.bat / package.bat / run_dev.bat 均为纯 ASCII
MSYS_NO_PATHCONV=1 cmd /c "tools\build.bat"  → [BUILD OK]（改成 QT_DIR/VS_DIR 可覆盖后重跑通过）
MSYS_NO_PATHCONV=1 cmd /c "tools\package.bat" → [PACKAGE OK] dist/DesktopWidget-0.3.0.zip
```

## 版本控制

本版提交信息：`V0.3.0: 首个公开版本（贴壁纸层的下班倒计时 + 纪念日桌面组件）`。
标签 `v0.3.0` 与 Release 见 `CHANGELOG.md`。
