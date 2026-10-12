"""诊断：对比"用户屏幕截图里的组件"与"程序自渲染"的清晰度，并测出参考面板（收集箱）实际宽度。

用法： python tools/diag_sharpness.py <屏幕截图.png>
"""
import os
import sys

from PIL import Image
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _paths  # noqa: E402

if len(sys.argv) < 2:
    sys.exit("用法: python tools/diag_sharpness.py <屏幕截图.png>")
u = Image.open(sys.argv[1]).convert("RGB")
print("用户截图尺寸:", u.size)
a = np.array(u)

# 1) 量卡片实际宽度（底部区域的近纯白像素）
bottom = a[1000:, :, :]
mask = (bottom[:, :, 0] > 245) * (bottom[:, :, 1] > 245) * (bottom[:, :, 2] > 245)
cols = np.where(mask.sum(axis=0) > 50)[0]
rows = np.where(mask.sum(axis=1) > 50)[0]
if len(cols):
    print(f"卡片白色区域: x {cols.min()}..{cols.max()} → 宽 {cols.max()-cols.min()+1} px（物理像素）")
    print(f"             y {rows.min()+1000}..{rows.max()+1000} → 高 {rows.max()-rows.min()+1} px")

# 2) 量收集箱面板宽度（右上深色面板）
top = a[:800, :, :]
dark = (top[:, :, 0] < 60) * (top[:, :, 1] < 70) * (top[:, :, 2] < 70)
dcols = np.where(dark.sum(axis=0) > 200)[0]
if len(dcols):
    print(f"收集箱面板: x {dcols.min()}..{dcols.max()} → 宽 {dcols.max()-dcols.min()+1} px")


# 3) 清晰度指标（Laplacian 方差，越大越锐）
def focus(im):
    g = np.asarray(im.convert("L"), dtype=float)
    lap = -4 * g[1:-1, 1:-1] + g[:-2, 1:-1] + g[2:, 1:-1] + g[1:-1, :-2] + g[1:-1, 2:]
    return float(lap.var())


card_digits = u.crop((120, 925, 500, 1020))
panel_text = u.crop((360, 200, 700, 480))
print(f"\n清晰度(Laplacian 方差)：屏幕上的卡片数字 {focus(card_digits):.1f} / 收集箱文字 {focus(panel_text):.1f}")

s = Image.open(os.path.join(_paths.data_dir(), "self-shot.png")).convert("RGBA")
print("程序自渲染尺寸:", s.size)
flat = Image.new("RGB", s.size, (255, 255, 255))
flat.paste(s, mask=s.split()[3])
print(f"清晰度：程序自渲染数字区 {focus(flat.crop((30, 55, 320, 150))):.1f}")
