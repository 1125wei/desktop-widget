"""改配置文件的单个字段： python tools/setcfg.py <key> <value>

value 支持 int / float / true / false / 字符串（按顶层字段改，卡片字段请用 --card <索引>）。
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _paths  # noqa: E402

p = _paths.config_path()
if len(sys.argv) < 3:
    sys.exit(f"用法: python tools/setcfg.py <key> <value>\n  配置文件: {p}")

key, raw = sys.argv[1], sys.argv[2]

if raw.lower() in ("true", "false"):
    val = raw.lower() == "true"
else:
    try:
        val = int(raw)
    except ValueError:
        try:
            val = float(raw)
        except ValueError:
            val = raw

d = json.load(open(p, encoding="utf-8"))
old = d.get(key, "<不存在>")
d[key] = val
json.dump(d, open(p, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
back = json.load(open(p, encoding="utf-8"))
print(f"{key}: {old} → {back[key]}   (写入校验 {'OK' if back[key] == val else '失败!'})")
