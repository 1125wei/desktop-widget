"""把用户给的卡通猫源图处理成可用的透明底素材：assets/mascot.png

为什么要处理：
  1) 源图是白底（没有 alpha），直接放卡片上会是一个白方块；
  2) 不能简单"把白色变透明" —— 猫的**肚子也是白的**，那样会把身体掏空。
     正确做法是**从图像四边向内泛洪（flood fill）**：只把与边界连通的白像素变透明，
     肚子被深棕描边围住，泛洪进不去 → 保留不透明。

用法：python tools/prepare_mascot.py <源图> [输出图]
"""
import collections
import os
import sys

from PIL import Image

WHITE_MIN = 228   # 判定"背景白"的下限。源图可能是 JPEG（压缩噪点会让本该是白的像素降到 ~235），
                  # 所以阈值不能卡太高，否则会留下"墙壁"把背景切成一块块、泛洪过不去 → 留下斑点。
                  # 安全性来自"角色有深棕描边包围"，而不是阈值本身。
OUT_DEFAULT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "mascot.png")


def is_bgish(p):
    r, g, b, a = p
    if a == 0:
        return True
    return r >= WHITE_MIN and g >= WHITE_MIN and b >= WHITE_MIN and max(r, g, b) - min(r, g, b) <= 12


def main():
    if len(sys.argv) < 2:
        sys.exit("用法: python tools/prepare_mascot.py <源图> [输出图]\n"
                 "  例: python tools/prepare_mascot.py D:/我的猫.png assets/mascot.png")
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else OUT_DEFAULT
    im = Image.open(src).convert("RGBA")
    w, h = im.size
    px = im.load()

    # 从四边泛洪，标记"外部背景"
    outside = bytearray(w * h)
    q = collections.deque()
    for x in range(w):
        for y in (0, h - 1):
            if not outside[y * w + x] and is_bgish(px[x, y]):
                outside[y * w + x] = 1
                q.append((x, y))
    for y in range(h):
        for x in (0, w - 1):
            if not outside[y * w + x] and is_bgish(px[x, y]):
                outside[y * w + x] = 1
                q.append((x, y))
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < w and 0 <= ny < h and not outside[ny * w + nx] and is_bgish(px[nx, ny]):
                outside[ny * w + nx] = 1
                q.append((nx, ny))

    cleared = 0
    for y in range(h):
        for x in range(w):
            if outside[y * w + x]:
                px[x, y] = (0, 0, 0, 0)
                cleared += 1

    # 只保留"最大的连通块"（猫本体），其余零散没清干净的噪点/残块全部丢掉。
    # 这一步对 JPEG 源（棋盘格 + 压缩噪点）特别关键：阈值再宽也会漏下一些孤立小块，
    # 它们会让 crop 的外框撑到整张画布，还会在卡片上显示成小灰点。
    seen = bytearray(w * h)
    comps = []
    for y0 in range(h):
        for x0 in range(w):
            i0 = y0 * w + x0
            if seen[i0] or px[x0, y0][3] == 0:
                continue
            q2 = collections.deque([(x0, y0)])
            seen[i0] = 1
            comp = []
            while q2:
                cx, cy = q2.popleft()
                comp.append((cx, cy))
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = cx + dx, cy + dy
                    if 0 <= nx < w and 0 <= ny < h and not seen[ny * w + nx] and px[nx, ny][3] > 0:
                        seen[ny * w + nx] = 1
                        q2.append((nx, ny))
            comps.append(comp)
    dropped = 0
    if comps:
        comps.sort(key=len, reverse=True)
        for comp in comps[1:]:
            for (cx, cy) in comp:
                px[cx, cy] = (0, 0, 0, 0)
            dropped += len(comp)
        print(f"  连通块 {len(comps)} 个 → 保留最大的一块（{len(comps[0])} 像素），丢掉 {dropped} 像素的碎块")

    # 裁到内容外框（留 2% 边距）
    bbox = im.getbbox()
    if bbox:
        pad = int(max(bbox[2] - bbox[0], bbox[3] - bbox[1]) * 0.02)
        im = im.crop((max(0, bbox[0] - pad), max(0, bbox[1] - pad),
                      min(w, bbox[2] + pad), min(h, bbox[3] + pad)))

    os.makedirs(os.path.dirname(out), exist_ok=True)
    im.save(out)
    # 统计：内部白是否保住了
    interior = im.convert("RGBA")
    opaque = sum(1 for p in interior.getdata() if p[3] > 200)
    print(f"源图 {src}")
    print(f"  泛洪清掉 {cleared} 像素（{cleared * 100.0 / (w * h):.1f}% 是外部白底）")
    print(f"  裁到 {im.size}，不透明像素 {opaque} 个 → 写出 {out}")


if __name__ == "__main__":
    main()
