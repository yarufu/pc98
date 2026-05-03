#!/usr/bin/env python3
"""
640x400 / 16色の indexed BMP/PNG を PC-98 向け .g98 へ変換します。

前提:
    - 入力画像は 640x400
    - 入力画像は P mode の indexed image
    - パレット番号 0〜15 をそのまま PC-98 の色番号として使う
    - このスクリプトでは減色しない
    - 透過したい部分は、あらかじめパレット番号 0 にしておく

.g98 の構造:
    0x00: 4 bytes  magic    "G98B"
    0x04: 2 bytes  width    640 (little endian)
    0x06: 2 bytes  height   400 (little endian)
    0x08: 1 byte   version  1
    0x09: 4 bytes  planes   "BRGI"
    0x0D: plane B data (80 * 400 bytes)
    続いて plane R, G, I

各 plane は PC-98 VRAM と同じ並びです。
1 byte で横 8 pixel を持ち、bit7 が左端です。

使い方:
    python3 tools/png_to_g98.py input.bmp output.g98

重要:
    - このスクリプトは色変換しません
    - 入力画像の index 0..15 をそのまま使います
    - C 側で color == 0 を描かなければ透過になります
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from PIL import Image


WIDTH = 640
HEIGHT = 400
BYTES_PER_LINE = WIDTH // 8


def load_indices(image: Image.Image) -> list[int]:
    if image.size != (WIDTH, HEIGHT):
        raise ValueError(f"input image must be {WIDTH}x{HEIGHT}")

    if image.mode != "P":
        raise ValueError(
            "input image must be indexed color (P mode). "
            "Convert it to 16-color indexed BMP/PNG first."
        )

    data = list(image.getdata())
    if not data:
        return data

    min_index = min(data)
    max_index = max(data)

    if min_index < 0 or max_index > 15:
        raise ValueError(
            f"palette indices must be in range 0..15, but got {min_index}..{max_index}"
        )

    return data


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


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert 640x400 indexed 16-color BMP/PNG to PC-98 .g98"
    )
    parser.add_argument("input", help="input indexed BMP/PNG file")
    parser.add_argument("output", help="output .g98 file")
    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)

    if not input_path.exists():
        raise FileNotFoundError(f"input file not found: {input_path}")

    with Image.open(input_path) as image:
        indices = load_indices(image)

    write_g98(indices, output_path)
    print(f"wrote {output_path}")


if __name__ == "__main__":
    main()
