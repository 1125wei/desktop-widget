"""清晰度诊断 v2：定位两张图里的卡片，用**相同的相对区域**（按卡片宽高换算）裁切后对比清晰度。

- 屏幕截图（含壁纸）：只在图像下半部找卡片（避开其它白色 UI）
- 程序自渲染（透明 PNG）：合成到灰蓝底上找卡片，全图扫描

用法： python tools/diag_sharpness2.py <屏幕截图.png>
"""
import glob
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _paths  # noqa: E402

if len(sys.argv) < 2:
    sys.exit("用法: python tools/diag_sharpness2.py <屏幕截图.png>")

BG = (122, 138, 153)  # 自渲染合成底色（只要不是白色，能和卡片区分开即可）


def focus(im):
    g = np.asarray(im.convert("L"), dtype=float)
    lap = -4 * g[1:-1, 1:-1] + g[:-2, 1:-1] + g[2:, 1:-1] + g[1:-1, :-2] + g[1:-1, 2:]
    return float(lap.var())


def card_bbox(img, from_y_ratio):
    """卡片 = 大片纯白块：行/列的白像素数量都要足够多（避开白色小控件/文字）"""
    a = np.array(img.convert("RGB"))
    y0 = int(a.shape[0] * from_y_ratio)
    sub = a[y0:, :, :]
    m = (sub[:, :, 0] > 246) * (sub[:, :, 1] > 246) * (sub[:, :, 2] > 246)
    cols = np.where(m.sum(axis=0) > 120)[0]
    rows = np.where(m.sum(axis=1) > 120)[0]
    if not len(cols) or not len(rows):
        return None
    return (int(cols.min()), int(rows.min()) + y0, int(cols.max()), int(rows.max()) + y0)


def report(tag, img):
    bb = card_bbox(img, 0.4 if img.width > 900 else 0.0)
    if not bb:
        print(f"{tag}: 未找到卡片")
        return
    w, h = bb[2] - bb[0] + 1, bb[3] - bb[1] + 1
    sx, sy = w / 470.0, h / 206.0
    crop = img.crop((bb[0] + int(30 * sx), bb[1] + int(55 * sy),
                     bb[0] + int(320 * sx), bb[1] + int(150 * sy)))
    print(f"{tag}\n    卡片 {w}x{h}px  数字区裁切 {crop.size}  清晰度 = {focus(crop):.1f}")


u = Image.open(sys.argv[1]).convert("RGB")
report("【改动前】屏幕上的旧版卡片（含描边）", u)

for p in sorted(glob.glob(os.path.join(_paths.data_dir(), "self-shot*.png"))):
    s = Image.open(p).convert("RGBA")
    flat = Image.new("RGB", s.size, BG)
    flat.paste(s, mask=s.split()[3])
    report("【对比】程序自渲染 " + os.path.basename(p), flat)
