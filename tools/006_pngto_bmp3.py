from PIL import Image
import random

# =========================
# 設定
# =========================

INPUT_FILE = "UI.png"
OUTPUT_FILE = "UI_3color_dither.png"

# 固定3色パレット
PALETTE = [
    (10, 10, 10),       # 黒
    (51, 102, 221),     # 濃青
    (153, 187, 204),    # 薄青
]

# ディザ強さ
# UIなら 8～16 くらいがおすすめ
DITHER_STRENGTH = 12


# =========================
# 色距離計算
# =========================

def color_distance_sq(c1, c2):
    r1, g1, b1 = c1
    r2, g2, b2 = c2
    return (r1 - r2) ** 2 + (g1 - g2) ** 2 + (b1 - b2) ** 2


def nearest_color(pixel, palette):
    best = palette[0]
    best_dist = color_distance_sq(pixel, best)

    for color in palette[1:]:
        dist = color_distance_sq(pixel, color)
        if dist < best_dist:
            best_dist = dist
            best = color

    return best


# =========================
# 自作ディザ
# =========================

def clamp(v):
    return max(0, min(255, v))


def dithered_pixel(pixel, strength):
    r, g, b = pixel

    # 各チャンネルに少しだけランダムノイズを加える
    # 強すぎるとUIが崩れるので弱め推奨
    r = clamp(r + random.randint(-strength, strength))
    g = clamp(g + random.randint(-strength, strength))
    b = clamp(b + random.randint(-strength, strength))

    return (r, g, b)


# =========================
# メイン処理
# =========================

def main():
    img = Image.open(INPUT_FILE).convert("RGB")
    pixels = img.load()

    width, height = img.size

    for y in range(height):
        for x in range(width):
            original = pixels[x, y]

            # ノイズを加えてから最近傍色に丸める
            noisy = dithered_pixel(original, DITHER_STRENGTH)
            mapped = nearest_color(noisy, PALETTE)

            pixels[x, y] = mapped

    img.save(OUTPUT_FILE)
    print(f"保存しました: {OUTPUT_FILE}")


if __name__ == "__main__":
    main()
