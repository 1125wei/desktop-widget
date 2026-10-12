"""卡通猫形象预览渲染器（不依赖 Qt）。

设计稿 120x120，与 qml/Mascot.qml 里的 Canvas 绘制代码**用同一套路径数据**，
所以这里看到什么，卡片上就是什么。改完形后把数字抄回 Mascot.qml。
用法：python tools/mascot_preview.py [输出png]
"""
import sys
from PIL import Image, ImageDraw

SS = 4  # 超采样倍数（先 4 倍画再缩小 → 抗锯齿）
DESIGN = 120
OUT = (110, 75, 51)
FUR = (217, 188, 156)
WHT = (255, 255, 255)
BLU = (61, 95, 168)
BG = (244, 242, 238)


def bez(p0, p1, p2, p3, n=48):
    """三次贝塞尔采样成点列（含端点）。"""
    pts = []
    for i in range(n + 1):
        t = i / n
        u = 1 - t
        x = u**3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t**3 * p3[0]
        y = u**3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t**3 * p3[1]
        pts.append((x, y))
    return pts


def poly(*segs):
    """把一串线段/贝塞尔拼成点列。seg 形如 ('L',x,y) 或 ('C',cx1,cy1,cx2,cy2,x,y)。"""
    pts = []
    cur = None
    for s in segs:
        if s[0] == "M":
            cur = (s[1], s[2])
            pts.append(cur)
        elif s[0] == "L":
            cur = (s[1], s[2])
            pts.append(cur)
        elif s[0] == "C":
            p1, p2, p3 = (s[1], s[2]), (s[3], s[4]), (s[5], s[6])
            seg = bez(cur, p1, p2, p3)
            pts.extend(seg[1:])
            cur = p3
    return pts


def scaled(pts, k, off):
    return [(off + x * k, off + y * k) for x, y in pts]


def stroke_poly(d, pts, color, w):
    d.line(pts, fill=color, width=max(1, int(round(w))), joint="curve")
    r = w / 2.0
    for p in (pts[0], pts[-1]):  # 圆头
        d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=color)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else r"D:\desktop-widget\tools\mascot-preview.png"
    N = DESIGN * SS
    img = Image.new("RGB", (N, N), BG)
    d = ImageDraw.Draw(img)
    k = SS
    off = 0

    # 参考线
    d.line([(0, 60 * k), (N, 60 * k)], fill=(221, 216, 208))
    d.line([(60 * k, 0), (60 * k, N)], fill=(221, 216, 208))

    # 1) 尾巴（细一点、从身体里探出来）
    tail = poly(("M", 44, 92), ("C", 34, 96, 26, 103, 12, 99))
    stroke_poly(d, scaled(tail, k, off), OUT, 5 * k)
    stroke_poly(d, scaled(tail, k, off), FUR, 2 * k)

    # 2) 身体（白色；顶部藏进绒毛里，避免出现尖尖的蛋形）
    body = poly(
        ("M", 37, 40),
        ("C", 28, 56, 26, 78, 34, 90),
        ("C", 38, 98, 44, 103, 50, 103),
        ("C", 56, 103, 62, 98, 66, 90),
        ("C", 74, 78, 72, 56, 63, 40),
    )
    bp = scaled(body + [body[0]], k, off)
    d.polygon(bp, fill=WHT)
    d.line(bp, fill=OUT, width=4 * k, joint="curve")

    # 3) 头/背（卡其；宽而圆，两只大耳 + 浅扇贝下缘）
    mane = poly(
        ("M", 18, 64),
        ("C", 6, 54, 6, 32, 18, 26),
        ("C", 22, 14, 30, 9, 38, 12),
        ("C", 45, 15, 48, 21, 49, 27),
        ("C", 50, 21, 53, 14, 60, 12),
        ("C", 68, 9, 76, 14, 80, 26),
        ("C", 92, 32, 92, 54, 80, 64),
        ("L", 75, 61),
        ("L", 70, 64),
        ("L", 64, 61),
        ("L", 58, 64),
        ("L", 52, 61),
        ("L", 46, 64),
        ("L", 40, 61),
        ("L", 34, 64),
        ("L", 28, 61),
    )
    mp = scaled(mane + [mane[0]], k, off)
    d.polygon(mp, fill=FUR)
    d.line(mp, fill=OUT, width=4 * k, joint="curve")

    # 4) 蓝标记
    stroke_poly(d, scaled([(45, 33), (48, 46)], k, off), BLU, 6 * k)
    stroke_poly(d, scaled([(55, 31), (58, 44)], k, off), BLU, 6 * k)

    # 5) 小波浪（绒毛上、蓝标记下方）
    wave = poly(("M", 44, 47), ("C", 47, 54, 50, 54, 52, 48), ("C", 54, 54, 57, 54, 60, 47))
    stroke_poly(d, scaled(wave, k, off), OUT, 3 * k)

    img = img.resize((DESIGN * 4, DESIGN * 4), Image.LANCZOS)
    img.save(out)
    print("已渲染预览:", out, img.size)


if __name__ == "__main__":
    main()
