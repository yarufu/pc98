#!/usr/bin/env python3
"""
PNGをPC-98 ADV用の .g98 に一発変換します。

入力:
    PNG画像

出力:
    .g98
    確認用BMP（自動で *_preview.bmp）

使い方:
    python3 tools/003_png_to_g98.py input.png bg001.g98

前提:
    - 出力サイズは 640x400 固定
    - 固定16色パレットへ減色
    - パレット番号 0〜15 をそのまま .g98 に入れる
    - 0番は透過専用として予約
    - 見える黒は 2番へ逃がす
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from PIL import Image


WIDTH = 640
HEIGHT = 400
BYTES_PER_LINE = WIDTH // 8
VISIBLE_BLACK_INDEX = 2


# ===== PC-98 ADV用 固定16色パレット =====
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


def build_palette_bytes() -> list[int]:
    palette: list[int] = []

    for color in pc98_palette:
        palette.extend(color)

    palette += [0] * (768 - len(palette))
    return palette


def build_palette_image() -> Image.Image:
    palette_img = Image.new("P", (1, 1))
    palette_img.putpalette(build_palette_bytes())
    return palette_img


def resize_to_pc98(img: Image.Image) -> Image.Image:
    # Pillowのバージョン差対策
    try:
        resample = Image.Resampling.LANCZOS
    except AttributeError:
        resample = Image.LANCZOS

    return img.resize((WIDTH, HEIGHT), resample)


def png_to_indexed16(input_path: Path, alpha_threshold: int = 128) -> Image.Image:
    """
    PNGを640x400のPモード16色画像に変換する。
    透明部分はindex 0へ、見える黒はindex 2へ逃がす。
    """
    img_rgba = Image.open(input_path).convert("RGBA")
    img_rgba = resize_to_pc98(img_rgba)

    alpha = img_rgba.getchannel("A")
    transparent_mask = alpha.point(lambda a: 255 if a < alpha_threshold else 0)

    img_rgb = img_rgba.convert("RGB")

    # RGB完全黒はパレット0に吸われやすいので、見える黒(index 2)へ逃がす
    pixels = list(img_rgb.getdata())
    escaped_pixels = []

    for r, g, b in pixels:
        if (r, g, b) == (0, 0, 0):
            escaped_pixels.append(pc98_palette[VISIBLE_BLACK_INDEX])
        else:
            escaped_pixels.append((r, g, b))

    img_rgb.putdata(escaped_pixels)

    img_p = img_rgb.quantize(
        palette=build_palette_image(),
        dither=Image.NONE,
    )

    # 透明部分は強制的に0番へ
    out_pixels = img_p.load()
    mask_pixels = transparent_mask.load()

    for y in range(HEIGHT):
        for x in range(WIDTH):
            if mask_pixels[x, y] != 0:
                out_pixels[x, y] = 0

    # 念のため、0〜15以外がないか確認
    data = list(img_p.getdata())
    min_index = min(data)
    max_index = max(data)
    if min_index < 0 or max_index > 15:
        raise ValueError(
            f"palette indices must be 0..15, but got {min_index}..{max_index}"
        )

    return img_p


def pack_plane(indices: list[int], bit_mask: int) -> bytes:
    plane = bytearray(BYTES_PER_LINE * HEIGHT)

    for y in range(HEIGHT):
        row_base = y * WIDTH
        dst_base = y * BYTES_PER_LINE

        for byte_x in range(BYTES_PER_LINE):
            value = 0
            src_base = row_base + byte_x * 8

            for bit in range(8):
                color = indices[src_base + bit]
                if color & bit_mask:
                    value |= 0x80 >> bit

            plane[dst_base + byte_x] = value

    return bytes(plane)


def write_g98(indices: list[int], output_path: Path) -> None:
    header = struct.pack(
        "<4sHHB4s",
        b"G98B",
        WIDTH,
        HEIGHT,
        1,
        b"BRGI",
    )

    blue = pack_plane(indices, 0x01)
    red = pack_plane(indices, 0x02)
    green = pack_plane(indices, 0x04)
    intens = pack_plane(indices, 0x08)

    with output_path.open("wb") as fp:
        fp.write(header)
        fp.write(blue)
        fp.write(red)
        fp.write(green)
        fp.write(intens)


def save_preview_bmp(img_p: Image.Image, output_g98_path: Path) -> Path:
    preview_path = output_g98_path.with_name(output_g98_path.stem + "_preview.bmp")

    # 念のためパレットを入れ直して保存
    img_p.putpalette(build_palette_bytes())
    img_p.save(preview_path, format="BMP")

    return preview_path


def convert_png_to_g98(input_path: Path, output_g98_path: Path) -> None:
    if not input_path.exists():
        raise FileNotFoundError(f"input file not found: {input_path}")

    img_p = png_to_indexed16(input_path)
    indices = list(img_p.getdata())

    write_g98(indices, output_g98_path)
    preview_path = save_preview_bmp(img_p, output_g98_path)

    print("OK")
    print(f"input       : {input_path}")
    print(f"g98 output  : {output_g98_path}")
    print(f"preview BMP : {preview_path}")
    print(f"size        : {WIDTH}x{HEIGHT}")
    print("palette     : fixed PC-98 16 colors")
    print("index 0     : reserved")
    print(f"visible black index: {VISIBLE_BLACK_INDEX}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert PNG to PC-98 ADV .g98 and preview BMP"
    )
    parser.add_argument("input", help="input PNG file")
    parser.add_argument("output", help="output .g98 file")
    args = parser.parse_args()

    convert_png_to_g98(Path(args.input), Path(args.output))


if __name__ == "__main__":
    main()
