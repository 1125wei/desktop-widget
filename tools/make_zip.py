"""把 dist/DesktopWidget 打成 dist/DesktopWidget-<版本>.zip（版本取自 CMakeLists.txt）。"""
import os
import re
import shutil
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "dist", "DesktopWidget")
DIST = os.path.join(ROOT, "dist")

ver = "0.0.0"
with open(os.path.join(ROOT, "CMakeLists.txt"), encoding="utf-8") as f:
    m = re.search(r"project\(DesktopWidget VERSION ([0-9.]+)", f.read())
    if m:
        ver = m.group(1)

out = os.path.join(DIST, f"DesktopWidget-{ver}.zip")
if os.path.exists(out):
    os.remove(out)

count = 0
total = 0
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for base, _dirs, files in os.walk(SRC):
        for name in files:
            p = os.path.join(base, name)
            rel = os.path.relpath(p, SRC)
            z.write(p, os.path.join("DesktopWidget", rel))
            count += 1
            total += os.path.getsize(p)

print(f"版本 {ver}")
print(f"zip: {out}")
print(f"文件 {count} 个，合计 {total/1024/1024:.1f} MB，压缩后 {os.path.getsize(out)/1024/1024:.1f} MB")
