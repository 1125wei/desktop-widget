"""生成"改动前 / 改动后"清晰度对照图（同尺寸、同相对区域，2 倍放大便于肉眼判断）。

用法： python tools/make_compare.py <改动前的截图>
      （改动后的图自动取程序自检产物 self-shot.png）
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _paths  # noqa: E402

if len(sys.argv) < 2:
    sys.exit("用法: python tools/make_compare.py <改动前的截图.png>")
OLD = sys.argv[1]
NEW = os.path.join(_paths.data_dir(), "self-shot.png")
OUT = os.path.join(_paths.docs_dir(), "sharpness-compare.png")
BG = (30, 32, 36)


def find_card(img, from_y_ratio):
    import numpy as np
    a = np.array(img.convert("RGB"))
    y0 = int(a.shape[0] * from_y_ratio)
    sub = a[y0:, :, :]
    m = (sub[:, :, 0] > 246) * (sub[:, :, 1] > 246) * (sub[:, :, 2] > 246)
    cols = np.where(m.sum(axis=0) > 120)[0]
    rows = np.where(m.sum(axis=1) > 120)[0]
    return (int(cols.min()), int(rows.min()) + y0, int(cols.max()), int(rows.max()) + y0)


def digits_crop(img, bb, zoom=2):
    w, h = bb[2] - bb[0] + 1, bb[3] - bb[1] + 1
    sx, sy = w / 470.0, h / 206.0
    c = img.crop((bb[0] + int(18 * sx), bb[1] + int(30 * sy),
                  bb[0] + int(330 * sx), bb[1] + int(160 * sy)))
    return c.resize((c.width * zoom, c.height * zoom), Image.NEAREST)


old_img = Image.open(OLD).convert("RGB")
new_img = Image.open(NEW).convert("RGBA")
flat = Image.new("RGB", new_img.size, (255, 255, 255))
flat.paste(new_img, mask=new_img.split()[3])

a = digits_crop(old_img, find_card(old_img, 0.4))
b = digits_crop(flat, find_card(flat, 0.0))

pad, label_h = 16, 26
W = max(a.width, b.width) + pad * 2
H = label_h + a.height + pad + label_h + b.height + pad
canvas = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(canvas)
try:
    font = ImageFont.truetype("arial.ttf", 18)
except OSError:
    font = ImageFont.load_default()

d.text((pad, 4), "BEFORE  (on-screen, outline + soft shadow)", fill=(255, 130, 130), font=font)
canvas.paste(a, (pad, label_h))
y = label_h + a.height + pad
d.text((pad, y), "AFTER  (new render, solid text + hairline edge)", fill=(130, 220, 150), font=font)
canvas.paste(b, (pad, y + label_h))
canvas.save(OUT)
print("对照图已生成:", OUT, canvas.size)
