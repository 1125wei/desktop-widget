#!/usr/bin/env bash
# 桌面组件 UI/交互 回归脚本（构建后的 canonical 验证入口）
#   1) 编辑模式端到端 + 位置持久化（多卡片：每张卡各自一条）
#   2) 编辑模式视觉
#   3) 第二次启动读配置（v1→v2 迁移也在这里暴露）
#   4) 层策略：auto/workerw/progman/bottom/float 各跑一遍，看 parent 是谁
#   5) 拖丢自愈（坐标越界 → 启动时钳制回屏内）
#   6) 编辑模式退出后必须仍可见（z 序 / 曝光回归）
#   7) 设置界面渲染 + 改动即时生效
#   8) 多卡片：新增纪念日卡 → 设置内容 → 抓渲染
#   9) 开机自启：写注册表 → 读回 → 复原
set -u
cd "$(dirname "$0")/.."
EXE="build/DesktopWidget.exe"
DATA="/c/Users/$(whoami)/AppData/Roaming/wei/DesktopWidget"
LOG="$DATA/widget.log"
CFG="$DATA/config.json"
QTBIN="${QT_DIR:-D:/Qt/6.8.3/msvc2022_64}/bin"

if [ ! -f "$EXE" ]; then
  echo "[ERROR] 没有 $EXE —— 先跑: MSYS_NO_PATHCONV=1 cmd /c \"tools\\\\build.bat\""
  exit 1
fi

run() { # run "ENV1=x ENV2=y"  —— 跑完指定毫秒数退出
  MSYS_NO_PATHCONV=1 cmd /c "set PATH=$QTBIN;%PATH% && $1 && build\\\\DesktopWidget.exe" >/dev/null 2>&1
}

echo "########## 1) 编辑模式端到端 + 位置持久化（每张卡）##########"
run "set DW_EDIT_TEST=1 && set DW_QUIT_MS=7000"
grep -a -E "托盘|卡片状态|\[card\]|\[edit\]|edit-test|还原位置|初始摆位" "$LOG" | head -25

echo
echo "########## 2) 编辑模式视觉（抓卡片自身 + 壁纸层）##########"
run "set DW_EDIT_TEST=1 && set DW_PROBE=1 && set DW_SHOT=1 && set DW_SHOT_MS=3500 && set DW_QUIT_MS=9500"
grep -a -E "self-shot|probe|layer content|layer-after-edit" "$LOG"

echo
echo "########## 3) 第二次启动读配置（含 v1→v2 迁移提示）##########"
run "set DW_QUIT_MS=3000"
grep -a -E "配置已加载|migrate|还原位置" "$LOG"

echo
echo "########## 4) 层策略（parent 判定）##########"
for mode in auto workerw progman bottom float; do
  run "set DW_LAYER=$mode && set DW_QUIT_MS=3000"
  printf "  %-8s → " "$mode"
  grep -a -oE "\[card\] 0 挂载: .*" "$LOG" | head -1
done

echo
echo "########## 5) 拖丢自愈（第 1 张卡坐标越界 → 启动时钳制回屏内）##########"
python - <<'PY'
import json, os
p = os.path.join(os.environ["APPDATA"], "wei", "DesktopWidget", "config.json")
d = json.load(open(p, encoding="utf-8"))
d["cards"][0]["x"], d["cards"][0]["y"], d["cards"][0]["hasPos"] = 5000, 3000, True
json.dump(d, open(p, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
print("  已把 cards[0] 位置改成 (5000,3000)")
PY
run "set DW_QUIT_MS=3000"
grep -a -E "\[card\] 0 还原位置" "$LOG" | tail -1
python -c "
import json,os
d=json.load(open(os.path.join(os.environ['APPDATA'],'wei','DesktopWidget','config.json'),encoding='utf-8'))
print('  落盘后位置:', [(c['cardName'], c['x'], c['y']) for c in d['cards']])
"

echo
echo "########## 6) 编辑模式：退出后必须仍然可见（z序/曝光回归）##########"
rm -f "$DATA"/layer-after-edit-*.png
run "set DW_EDIT_TEST=1 && set DW_QUIT_MS=9000"
grep -a -E "z序锚点|z序\[|isExposed|edit-test" "$LOG"
echo "  ↑ 必须出现：z序锚点=SHELLDLL_DefView、每张卡 isExposed = true、退出后抓到 layer-after-edit-<i>.png"
echo "  （人工核对：这些 PNG 里应有卡片）"

echo
echo "########## 7) 设置界面：渲染自检 + 改动即时生效 ##########"
run "set DW_SHOT_SETTINGS=1 && set DW_TEST_APPLY=1 && set DW_QUIT_MS=6000"
grep -a -E "设置界面|\[test\]" "$LOG"

echo
echo "########## 8) 多卡片：新增纪念日卡 + 改内容 ##########"
run "set DW_ADD_ANNIV=1 && set DW_SET_ANNIV=1 && set DW_QUIT_MS=6000"
grep -a -E "\[card\]|\[test\]|纪念日" "$LOG" | head -12
echo "  （产物：$DATA/add-anniv-1.png、$DATA/set-anniv-1.png）"

echo
echo "########## 9) 开机自启：写注册表 → 读回 → 复原 ##########"
run "set DW_AUTOSTART=1 && set DW_QUIT_MS=3000"
grep -a -E "autostart|开机自启" "$LOG"

echo
echo "########## 10) 纪念日：日期解析（宽松）+ 两种模式口径 ##########"
run "set DW_TEST_ANNIV=1 && set DW_FREEZE=2026-10-12T11:00:00 && set DW_QUIT_MS=3500"
grep -a "test-anniv" "$LOG"
echo "  ↑ 期望：5 种写法全部接受并归一化成 2027-03-05；2027-02-31 / abc / 空 被拒绝且配置不变；6/6 用例通过"

echo
echo "########## 11) 纪念日：模式切换 + 提醒 + 卡片渲染 ##########"
run "set DW_TEST_MODE=1 && set DW_FREEZE=2026-10-12T11:00:00 && set DW_QUIT_MS=4800"
grep -a -E "test-mode|纪念日模式:" "$LOG"
echo "  ↑ 期望：倒计时/2027-3-5 = 144 天；过去日期 → 0 天 + 红字提醒；每年 → 44 天；纪念日 → 8357 天"
echo "  （产物：$DATA/mode-countdown-1.png、$DATA/mode-anniversary-1.png）"

echo
echo "########## 12) 农历：节日/自定义农历日换算 ##########"
run "set DW_TEST_LUNAR=1 && set DW_FREEZE=2026-10-12T11:00:00 && set DW_QUIT_MS=3800"
grep -a "test-lunar" "$LOG"
echo "  ↑ 期望：春节→2027-02-06、除夕→2027-02-05、中秋→2027-09-15、重阳→2026-10-18、端午→2027-06-09；"
echo "         九月初九→2026-10-18、腊月廿三→2027-01-30、腊月三十→2027-02-05（月末）、正月初一→2027-02-06；用例 9/9"
echo "  （产物：$DATA/lunar-1.png）"

echo
echo "产物目录：$DATA"
echo "  widget.log / config.json / self-shot-*.png / layer-content-*.png / settings.png"
