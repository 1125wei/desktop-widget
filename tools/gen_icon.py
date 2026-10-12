"""生成托盘/应用图标 assets/tray.ico（多尺寸）。

用法： python tools/gen_icon.py
"""
import os
from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "tray.ico")
S = 256


def rounded(draw, box, r, fill):
    draw.rounded_rectangle(box, radius=r, fill=fill)


def main():
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # 深蓝底 + 圆角
    rounded(d, (8, 8, S - 8, S - 8), 52, (20, 38, 58, 255))
    # 白色卡片
    rounded(d, (44, 62, S - 44, S - 76), 26, (255, 255, 255, 255))
    # 卡片上的两行"文本条"
    rounded(d, (66, 92, 176, 108), 8, (150, 150, 150, 255))
    rounded(d, (66, 126, 132, 146), 8, (108, 108, 108, 255))
    # 四宫格
    for i in range(4):
        x = 48 + i * 42
        rounded(d, (x, 158, x + 34, 196), 8, (236, 236, 236, 255))
    # 右侧橙色圆点（像那只橘子猫）
    d.ellipse((176, 158, 214, 196), fill=(240, 160, 60, 255))
    # 底部蓝色进度条（时间流逝感）
    rounded(d, (44, S - 62, S - 44, S - 44), 9, (47, 136, 255, 235))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    img.save(OUT, sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print("已生成", OUT, os.path.getsize(OUT), "bytes")


if __name__ == "__main__":
    main()
