#!/usr/bin/env bash
# 逐一切换桌面层策略跑一遍并打印层信息（构建后的回归脚本，别放临时目录）
set -u
cd "$(dirname "$0")/.."
EXE="build/DesktopWidget.exe"
LOG="/c/Users/$(whoami)/AppData/Roaming/wei/DesktopWidget/widget.log"
if [ ! -f "$EXE" ]; then
  echo "[ERROR] 没有 $EXE —— 先跑 tools/build.bat"
  exit 1
fi
for mode in workerw progman bottom float; do
  echo "===== DW_LAYER=$mode ====="
  rm -f "$LOG"
  MSYS_NO_PATHCONV=1 cmd /c "set PATH=D:\\Qt\\6.8.3\\msvc2022_64\\bin;%PATH% && set DW_LAYER=$mode && set DW_QUIT_MS=4000 && build\\DesktopWidget.exe" >/dev/null 2>&1
  if [ -f "$LOG" ]; then
    grep -E "progman=|SetParent|fallback|desktop layer|after placement|self screenshot" "$LOG" || cat "$LOG"
  else
    echo "[warn] 没找到日志 $LOG"
  fi
done
