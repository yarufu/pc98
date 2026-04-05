#!/usr/bin/env python3

from PIL import Image
import sys
import os

# ===== あなたのPC-98パレット =====
pc98_palette = [
    (0, 0, 0),       # 0: 透過専用
    (255, 255, 255), # 1: 白
    (10, 10, 10),    # 2: 黒
    (255, 204, 187), # 3: 肌色 光
    (238, 170, 153), # 4: 肌色 影
    (170, 187, 255), # 5: 青 光
    (51, 102, 221),  # 6: 青 影
    (85, 102, 102),  # 7: 青 影(タイル)
    (255, 187, 102), # 8: 黄色 普通
    (187, 136, 136), # 9: 黄色 影
    (238, 0, 85),    # 10: 赤
    (85, 170, 153),  # 11: 緑 淡い
    (85, 153, 102),  # 12: 緑
    (238, 255, 204), # 13: 緑(光)
    (255, 153, 187), # 14: 淡い赤
    (153, 187, 204), # 15: UI用
]

VISIBLE_BLACK_INDEX = 2  # 0は透過なので、見える黒は2を使う


def color_distance(c1, c2):
    return (c1[0] - c2[0]) ** 2 + (c1[1] - c2[1]) ** 2 + (c1[2] - c2[2]) ** 2


def find_nearest_color(rgb):
    best_index = 0
    best_dist = 999999999

    for i, p in enumerate(pc98_palette):
        d = color_distance(rgb, p)
        if d < best_dist:
            best_dist = d
            best_index = i

    return best_index


def build_palette_bytes():
    palette = []
    for color in pc98_palette:
        palette.extend(color)

    palette += [0] * (768 - len(palette))
    return palette


def save_preview_bmp(indices, width, height, bmp_path):
    """
    SPRに入れた色番号そのままを確認できる16色BMP。
    透明(index 0) は黒のまま見える。
    """
    img_p = Image.new("P", (width, height))
    img_p.putpalette(build_palette_bytes())
    img_p.putdata(indices)
    img_p.save(bmp_path, format="BMP")


def save_checker_preview(indices, width, height, bmp_path, checker_size=8):
    """
    透明(index 0) の部分だけ市松模様で見える確認用BMPを出力する。
    非透明部分は元のパレット色のまま描く。
    市松模様は RGB 画像として出力する。
    """
    checker_a = (192, 192, 192)
    checker_b = (128, 128, 128)

    img_rgb = Image.new("RGB", (width, height))
    out_pixels = img_rgb.load()

    for y in range(height):
        for x in range(width):
            idx = indices[y * width + x]

            if idx == 0:
                block_x = x // checker_size
                block_y = y // checker_size
                if ((block_x + block_y) & 1) == 0:
                    out_pixels[x, y] = checker_a
                else:
                    out_pixels[x, y] = checker_b
            else:
                out_pixels[x, y] = pc98_palette[idx]

    img_rgb.save(bmp_path, format="BMP")


def convert_png_to_spr(input_path, output_spr_path, alpha_threshold=128):
    img = Image.open(input_path).convert("RGBA")
    width, height = img.size
    pixels = img.load()

    data = []

    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]

            if a < alpha_threshold:
                data.append(0)  # 透過
            else:
                idx = find_nearest_color((r, g, b))

                # 0は透過専用なので、見えるピクセルで0になったら黒代用へ逃がす
                if idx == 0:
                    idx = VISIBLE_BLACK_INDEX

                data.append(idx)

    # ===== .spr 保存 =====
    with open(output_spr_path, "wb") as f:
        f.write(b"SPR0")
        f.write(width.to_bytes(2, "little"))
        f.write(height.to_bytes(2, "little"))
        f.write((1).to_bytes(1, "little"))
        f.write(bytearray(data))

    # ===== 確認用BMP保存 =====
    base, _ = os.path.splitext(output_spr_path)

    output_preview_bmp = base + "_preview.bmp"
    output_checker_bmp = base + "_checker.bmp"

    save_preview_bmp(data, width, height, output_preview_bmp)
    save_checker_preview(data, width, height, output_checker_bmp)

    print(f"OK: {output_spr_path}")
    print(f"preview BMP : {output_preview_bmp}")
    print(f"checker BMP : {output_checker_bmp}")
    print(f"size: {width}x{height}")
    print(f"transparent index: 0")
    print(f"visible black index: {VISIBLE_BLACK_INDEX}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("使い方: python png_to_spr.py input.png output.spr")
        sys.exit(1)

    convert_png_to_spr(sys.argv[1], sys.argv[2])
