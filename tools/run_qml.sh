#!/usr/bin/env bash
# 开发模式运行包装：把 KEY=value 通过 **导出的环境变量** 传给程序（避免 cmd 引号/吞空格）
# 用法： tools/run_qml.sh DW_SHOT=1 DW_SHOT_MS=2400 DW_QUIT_MS=3800
set -u
cd "$(dirname "$0")/.."
for kv in "$@"; do
  export "$kv"
done
MSYS_NO_PATHCONV=1 cmd /c "tools\\run_dev.bat" >/dev/null 2>&1
