"""清晰度测量：对 PNG 指定区域算 Laplacian 方差（越大越锐）。

用法： python tools/measure_focus.py <png> <x1> <y1> <x2> <y2> [把透明合成到某色]
"""
import sys
from PIL import Image
import numpy as np


def focus(im):
    g = np.asarray(im.convert("L"), dtype=float)
    lap = -4 * g[1:-1, 1:-1] + g[:-2, 1:-1] + g[2:, 1:-1] + g[1:-1, :-2] + g[1:-1, 2:]
    return float(lap.var())


path = sys.argv[1]
x1, y1, x2, y2 = (int(v) for v in sys.argv[2:6])
im = Image.open(path).convert("RGBA")
flat = Image.new("RGB", im.size, (255, 255, 255))
flat.paste(im, mask=im.split()[3])
print(f"{path.split(chr(92))[-1]}  区域=({x1},{y1})-({x2},{y2})  尺寸={flat.size}  清晰度={focus(flat.crop((x1, y1, x2, y2))):.1f}")
