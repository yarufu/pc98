from PIL import Image
import sys
import os


pc98_palette = [
    (0, 0, 0),       # 0: 透過専用
    (255, 255, 255), # 1: 白

    (10, 10, 10),    # 2：黒

    # 肌（2色）
    (255, 204, 187), # 3: 肌色 光
    (238, 170, 153), # 4: 肌色 影

    # 服
    (170, 187, 255), # 5: 青 光
    (51, 102, 221),  # 6: 青 影
    (85, 102, 102),  # 7: 青 影(タイル)

    # シャツ
    (255, 187, 102), # 8: 黄色 普通
    (187, 136, 136), # 9: 黄色 影

    # リボン・髪の毛
    (238, 0, 85),    # 10: 赤
    

    # 葉っぱ
    (85, 170, 153),  # 11:緑 淡い
    (85, 153, 102),  # 12:緑 
    (238, 255, 204), # 13:緑(光)

    # じゅうたん
    (255, 153, 187),  # 14: 淡い赤

    # UI
    (153, 187, 204), # 15: UI用
]


def build_palette_image():
    palette_img = Image.new("P", (1, 1))
    palette = []

    for color in pc98_palette:
        palette.extend(color)

    # 256色分に拡張
    palette += [0] * (768 - len(palette))
    palette_img.putpalette(palette)
    return palette_img


def apply_fixed_palette_with_transparency(img_rgba, alpha_threshold=128):
    """
    RGBA画像を固定16色パレットに減色しつつ、
    alpha_threshold 未満の画素をパレット0番（透過）にする。
    """
    # 透過マスクを作る
    alpha = img_rgba.getchannel("A")
    transparent_mask = alpha.point(lambda a: 255 if a < alpha_threshold else 0)

    # 減色用にRGB化
    img_rgb = img_rgba.convert("RGB")

    pixels = list(img_rgb.getdata())

    new_pixels = []
    for (r, g, b) in pixels:
        if (r, g, b) == (0, 0, 0):
            new_pixels.append((10, 10, 10))  # 黒を逃がす
        else:
            new_pixels.append((r, g, b))

    img_rgb.putdata(new_pixels)

    # 固定パレットで減色
    palette_img = build_palette_image()
    img_p = img_rgb.quantize(
        palette=palette_img,
        dither=Image.NONE
    )

    # 透過部分をパレット0番に強制する
    pixels = img_p.load()
    mask_pixels = transparent_mask.load()
    width, height = img_p.size

    for y in range(height):
        for x in range(width):
            if mask_pixels[x, y] != 0:
                pixels[x, y] = 0

    return img_p


def convert_to_pc98_16color(input_path, output_path):
    # RGBAで開く（透過を保持）
    img = Image.open(input_path).convert("RGBA")

    # PC-98向け解像度に変換
    img = img.resize((640, 400), Image.Resampling.LANCZOS)

    # 固定16色＋透過対応
    img_16 = apply_fixed_palette_with_transparency(img)

    # BMPで保存
    img_16.save(output_path, format="BMP")

    print("変換完了")
    print(f"入力 : {input_path}")
    print(f"出力 : {output_path}")
    print("透過部分はパレット0番に設定しました")


def main():
    if len(sys.argv) != 3:
        print("使い方:")
        print("  python pc98_convert_min.py 入力.png 出力.bmp")
        return

    input_path = sys.argv[1]
    output_path = sys.argv[2]

    if not os.path.exists(input_path):
        print(f"エラー: 入力ファイルが見つかりません: {input_path}")
        return

    convert_to_pc98_16color(input_path, output_path)


if __name__ == "__main__":
    main()
