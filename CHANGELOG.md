# 更新日志（CHANGELOG）

| 版本 | 日期 | 类型 | 摘要 | 详细说明 |
| --- | --- | --- | --- | --- |
| v0.3.0 | 2026-10-12 | 首个公开版本 | 贴壁纸层的桌面组件：下班倒计时 + 纪念日（含农历节日）+ 卡通形象，绿色免安装 | [VERSION_V0.3.0.md](VERSION_V0.3.0.md) |

## 发布流程

```bash
# 1. 写文档：VERSION_V<版本号>.md（五段式）+ 本文件补一行
# 2. 跑验证：tools/verify_ui.sh（12 节）+ tools/build.bat + tools/package.bat
# 3. 提交 + 打标签
git add -A && git commit -m "V<版本号>: 一句话摘要"
git tag -a v<版本号> -m "DesktopWidget V<版本号>：一句话"
git push origin main --tags
# 4. 发布（附件 = 绿色包，显示名带版本与平台）
gh release create v<版本号> dist/DesktopWidget-<版本号>.zip \
  --title "DesktopWidget V<版本号> · 一句话" --notes-file <说明.md> --verify-tag
```

发版前必跑（"改了什么跑什么"）：

| 改动类型 | 必跑 |
| --- | --- |
| 任何代码改动 | `tools/build.bat` + `tools/verify_ui.sh` |
| 改卡片口径/日期算法 | `DW_TEST_ANNIV` `DW_TEST_MODE` `DW_TEST_LUNAR`（配 `DW_FREEZE` 钉死时间） |
| 改节日/农历数据 | `tools/gen_festivals.py` 重生成 + `DW_TEST_LUNAR` |
| 改界面 | `DW_SHOT` / `DW_SHOT_SETTINGS` 抓图核对 + `tools/verify_ui.sh` |
| 改打包/依赖 | `tools/package.bat` + **清空 PATH 跑打包产物**（自包含验证） |
