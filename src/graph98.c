#include "graph98.h"

#include "debug.h"

#ifndef USE_EGC
#define USE_EGC 0
#endif

#if USE_EGC
#include "egc98.h"
#endif

#include <stdio.h>
#include <stdint.h>

#define GRAPH98_BYTES_PER_LINE 80u
#define GRAPH98_VRAM_PLANE_SIZE ((uint16_t)(GRAPH98_BYTES_PER_LINE * GRAPH98_HEIGHT))

#define GRAPH98_VRAM_BLUE   ((volatile uint8_t __far *)0xA8000000UL)
#define GRAPH98_VRAM_RED    ((volatile uint8_t __far *)0xB0000000UL)
#define GRAPH98_VRAM_GREEN  ((volatile uint8_t __far *)0xB8000000UL)
#define GRAPH98_VRAM_INTENS ((volatile uint8_t __far *)0xE0000000UL)

#define GRAPH98_PORT_COLOR_MODE 0x006A

#define GRAPH98_PORT_PALETTE_INDEX 0x00A8
#define GRAPH98_PORT_PALETTE_G     0x00AA
#define GRAPH98_PORT_PALETTE_R     0x00AC
#define GRAPH98_PORT_PALETTE_B     0x00AE

#define GRAPH98_G98_MAGIC_0 'G'
#define GRAPH98_G98_MAGIC_1 '9'
#define GRAPH98_G98_MAGIC_2 '8'
#define GRAPH98_G98_MAGIC_3 'B'
#define GRAPH98_G98_VERSION 1u
#define GRAPH98_G98_CHUNK_LINES 32u
#define GRAPH98_G98_INTERLACE_PERIOD 2u
#define GRAPH98_G98_INTERLACE_BOTTOM_OFFSET 1u
#define GRAPH98_G98_INTERLACE_GROUP_LINES 32u
#define GRAPH98_G98_INTERLACE_SWEEP_LINES 8u

#define GRAPH98_SPRITE_CHUNK_LINES 32u
#define GRAPH98_SPRITE_MAX_WIDTH 256u
#define GRAPH98_SPRITE_INTERLACE_MAX_HEIGHT 300u
#define GRAPH98_SPRITE_INTERLACE_PERIOD 2u
#define GRAPH98_SPRITE_INTERLACE_BOTTOM_OFFSET 1u
#define GRAPH98_SPRITE_INTERLACE_GROUP_LINES 32u
#define GRAPH98_SPRITE_INTERLACE_SWEEP_LINES 8u

#define GRAPH98_IMAGE_WORK_SIZE \
    (GRAPH98_SPRITE_MAX_WIDTH * GRAPH98_SPRITE_CHUNK_LINES)

/* Image loaders are non-reentrant and share this buffer sequentially. */
static uint8_t graph98_image_work[GRAPH98_IMAGE_WORK_SIZE];

_Static_assert(GRAPH98_IMAGE_WORK_SIZE == 8192u,
               "image work buffer must be 8 KB");
_Static_assert(GRAPH98_BYTES_PER_LINE * GRAPH98_G98_CHUNK_LINES <=
                   GRAPH98_IMAGE_WORK_SIZE,
               "G98 chunk exceeds image work buffer");
_Static_assert(GRAPH98_BYTES_PER_LINE * GRAPH98_G98_INTERLACE_GROUP_LINES <=
                   GRAPH98_IMAGE_WORK_SIZE,
               "G98 interlace group exceeds image work buffer");
_Static_assert(GRAPH98_SPRITE_MAX_WIDTH *
                   GRAPH98_SPRITE_INTERLACE_GROUP_LINES <=
                   GRAPH98_IMAGE_WORK_SIZE,
               "sprite interlace group exceeds image work buffer");

#define GRAPH98_SPRITE_MAGIC_0 'S'
#define GRAPH98_SPRITE_MAGIC_1 'P'
#define GRAPH98_SPRITE_MAGIC_2 'R'
#define GRAPH98_SPRITE_MAGIC_3 '0'
#define GRAPH98_SPRITE_VERSION 1u

#define GDC_MASTER_STATUS 0x00A0
#define GDC_STATUS_VSYNC 0x20

static uint8_t io_in8(uint16_t port)
{
    uint8_t value;

    __asm__ __volatile__(
        "inb %%dx, %%al"
        : "=a"(value)
        : "d"(port)
        : "cc");

    return value;
}

struct graph98_g98_header {
    uint8_t magic[4];
    uint16_t width;
    uint16_t height;
    uint8_t version;
    uint8_t plane_order[4];
};

/*
 * 立ち絵用の簡単なスプライト形式です。
 *
 * ファイル構造:
 *   0..3   : 'S' 'P' 'R' '0'
 *   4..5   : 幅   (little endian)
 *   6..7   : 高さ (little endian)
 *   8      : バージョン (今は 1)
 *   9..    : 1ピクセル1バイトの色番号データ
 *
 * 色番号は 0〜15 を使います。
 * 透過したい色は呼び出し側から渡します。
 */
struct graph98_sprite_header {
    uint8_t magic[4];
    uint16_t width;
    uint16_t height;
    uint8_t version;
};

/*
 * 5x7 ドットの数字フォントです。
 *
 * 各行は 5 ビットぶんだけ使います。
 * 左端のドットがビット 4、右端のドットがビット 0 です。
 *
 * 今回は最小版なので、数字 0〜9 だけを用意します。
 */
static const uint8_t graph98_digit_font[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, /* 0 */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* 1 */
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, /* 2 */
    {0x1E, 0x01, 0x01, 0x06, 0x01, 0x01, 0x1E}, /* 3 */
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, /* 4 */
    {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}, /* 5 */
    {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}, /* 6 */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* 7 */
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, /* 8 */
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}  /* 9 */
};

/*
 * 5x7 ドットの英字フォントです。
 *
 * 今回は初心者向けの最小版として、
 * A〜Z だけを固定配列で持ちます。
 *
 * 各行は 5 ビットぶんだけ使います。
 * 左端のドットがビット 4、右端のドットがビット 0 です。
 */
static const uint8_t alpha_font[26][7] = {
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* A */
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, /* B */
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, /* C */
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, /* D */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, /* E */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, /* F */
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}, /* G */
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* H */
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* I */
    {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E}, /* J */
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, /* K */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, /* L */
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, /* M */
    {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}, /* N */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* O */
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, /* P */
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, /* Q */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, /* R */
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, /* S */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* T */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* U */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, /* V */
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}, /* W */
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, /* X */
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, /* Y */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}  /* Z */
};

static void graph98_out8(uint16_t port, uint8_t value)
{
    __asm__ __volatile__(
        "outb %%al, %%dx"
        :
        : "a"(value), "d"(port)
        : "cc");
}

static uint8_t graph98_rgb8_to_4bit(uint8_t c)
{
    return (uint8_t)((c * 15u + 127u) / 255u);
}

static void graph98_set_palette_rgb8(uint8_t index,
                                     uint8_t r, uint8_t g, uint8_t b)
{
    graph98_out8(GRAPH98_PORT_PALETTE_INDEX, (uint8_t)(index & 0x0F));
    graph98_out8(GRAPH98_PORT_PALETTE_G, graph98_rgb8_to_4bit(g));
    graph98_out8(GRAPH98_PORT_PALETTE_R, graph98_rgb8_to_4bit(r));
    graph98_out8(GRAPH98_PORT_PALETTE_B, graph98_rgb8_to_4bit(b));
}

int graph98_load_palette_file(const char *path)
{
    FILE *fp;
    int i;

    fp = fopen(path, "r");
    if (fp == 0) {
        return 0;
    }

    for (i = 0; i < 16; ++i) {
        int r;
        int g;
        int b;

        /*
         * 1行に「R G B」の3つの整数を書く形式です。
         * 例:
         * 255 255 255
         */
        if (fscanf(fp, "%d %d %d", &r, &g, &b) != 3) {
            fclose(fp);
            return 0;
        }

        if (r < 0)   r = 0;
        if (r > 255) r = 255;
        if (g < 0)   g = 0;
        if (g > 255) g = 255;
        if (b < 0)   b = 0;
        if (b > 255) b = 255;

        graph98_set_palette_rgb8((uint8_t)i,
                                 (uint8_t)r,
                                 (uint8_t)g,
                                 (uint8_t)b);
    }

    fclose(fp);
    return 1;
}

void graph98_apply_adv_palette(void)
{
    /* Python側 pc98_palette と完全一致 */

    graph98_set_palette_rgb8( 0,   0,   0,   0);   /* 0: 透過用 */

    graph98_set_palette_rgb8( 1, 255, 255, 255);   /* 1: 白 */

    graph98_set_palette_rgb8( 2,  10,  10,  10);   /* 2: 黒 */

    graph98_set_palette_rgb8( 3, 255, 204, 187);   /* 3: 肌 光 */
    graph98_set_palette_rgb8( 4, 238, 170, 153);   /* 4: 肌 影 */

    graph98_set_palette_rgb8( 5, 170, 187, 255);   /* 5: 青 光 */
    graph98_set_palette_rgb8( 6,  51, 102, 221);   /* 6: 青 影 */
    graph98_set_palette_rgb8( 7,  85, 102, 102);   /* 7: 青 影(タイル) */

    graph98_set_palette_rgb8( 8, 255, 187, 102);   /* 8: 黄色 */
    graph98_set_palette_rgb8( 9, 187, 136, 136);   /* 9: 黄色 影 */

    graph98_set_palette_rgb8(10, 238,   0,  85);   /* 10: 赤 */

    graph98_set_palette_rgb8(11,  85, 170, 153);   /* 11: 緑 淡い */
    graph98_set_palette_rgb8(12,  85, 153, 102);   /* 12: 緑 */
    graph98_set_palette_rgb8(13, 238, 255, 204);   /* 13: 緑 光 */

    graph98_set_palette_rgb8(14, 255, 153, 187);   /* 14: ピンク */

    graph98_set_palette_rgb8(15, 153, 187, 204);   /* 15: UI */
}


static int graph98_copy_plane_from_file(FILE *fp, volatile uint8_t __far *plane)
{
    uint16_t y;

    y = 0;
    while (y < GRAPH98_HEIGHT) {
        uint16_t lines;
        uint16_t bytes_to_read;
        uint16_t offset;
        uint16_t i;

        lines = (uint16_t)(GRAPH98_HEIGHT - y);
        if (lines > GRAPH98_G98_CHUNK_LINES) {
            lines = GRAPH98_G98_CHUNK_LINES;
        }

        bytes_to_read = (uint16_t)(GRAPH98_BYTES_PER_LINE * lines);
        if (fread(graph98_image_work, 1u, bytes_to_read, fp) != bytes_to_read) {
            return 0;
        }

        offset = (uint16_t)(y * GRAPH98_BYTES_PER_LINE);
        for (i = 0; i < bytes_to_read; ++i) {
            plane[offset + i] = graph98_image_work[i];
        }

        y = (uint16_t)(y + lines);
    }

    return 1;
}

static void graph98_set_mode_640x400_16color(void)
{
    __asm__ __volatile__(
        "movb $0x42, %%ah\n\t"
        "movb $0xC0, %%ch\n\t"
        "int $0x18"
        :
        :
        : "ax", "cx", "cc", "memory");
}

static void graph98_display_on(void)
{
    __asm__ __volatile__(
        "movb $0x40, %%ah\n\t"
        "int $0x18"
        :
        :
        : "ax", "cc", "memory");
}

static void graph98_clear_plane(volatile uint8_t __far *plane)
{
    uint16_t i;

    for (i = 0; i < GRAPH98_VRAM_PLANE_SIZE; ++i) {
        plane[i] = 0x00;
    }
}

static void graph98_clear_vram(void)
{
    graph98_clear_plane(GRAPH98_VRAM_BLUE);
    graph98_clear_plane(GRAPH98_VRAM_RED);
    graph98_clear_plane(GRAPH98_VRAM_GREEN);
    graph98_clear_plane(GRAPH98_VRAM_INTENS);
}

void graph98_init(void)
{
    /*
     * T98-Next で動作確認済み。
     * ただし、実機では機種差の可能性があるため、
     * 同じ BIOS 呼び出しや I/O ポート設定がそのまま通らない場合があります。
     */
    graph98_set_mode_640x400_16color();
    graph98_display_on();
    graph98_out8(GRAPH98_PORT_COLOR_MODE, 0x01);
    graph98_clear_vram();
}

void graph98_pset(int x, int y, unsigned char color)
{
    uint16_t offset;
    uint8_t mask;

    if (x < 0 || x >= GRAPH98_WIDTH || y < 0 || y >= GRAPH98_HEIGHT) {
        return;
    }

    offset = (uint16_t)(y * GRAPH98_BYTES_PER_LINE + (x >> 3));
    mask = (uint8_t)(0x80u >> (x & 7));
    color &= 0x0Fu;

    if (color & 0x01u) {
        GRAPH98_VRAM_BLUE[offset] |= mask;
    } else {
        GRAPH98_VRAM_BLUE[offset] &= (uint8_t)~mask;
    }

    if (color & 0x02u) {
        GRAPH98_VRAM_RED[offset] |= mask;
    } else {
        GRAPH98_VRAM_RED[offset] &= (uint8_t)~mask;
    }

    if (color & 0x04u) {
        GRAPH98_VRAM_GREEN[offset] |= mask;
    } else {
        GRAPH98_VRAM_GREEN[offset] &= (uint8_t)~mask;
    }

    if (color & 0x08u) {
        GRAPH98_VRAM_INTENS[offset] |= mask;
    } else {
        GRAPH98_VRAM_INTENS[offset] &= (uint8_t)~mask;
    }
}

static void graph98_draw_sprite_line_trans_fast(const uint8_t *src,
                                                int src_start,
                                                int width,
                                                int dst_x,
                                                int dst_y,
                                                unsigned char transparent_color)
{
    int i;

    i = 0;
    while (i < width) {
        uint16_t offset;
        uint8_t bit;
        uint8_t draw_mask;
        uint8_t blue_bits;
        uint8_t red_bits;
        uint8_t green_bits;
        uint8_t intens_bits;

        offset = (uint16_t)(dst_y * GRAPH98_BYTES_PER_LINE + ((dst_x + i) >> 3));
        bit = (uint8_t)(0x80u >> ((dst_x + i) & 7));

        draw_mask = 0;
        blue_bits = 0;
        red_bits = 0;
        green_bits = 0;
        intens_bits = 0;

        while (i < width && bit != 0) {
            unsigned char color;

            color = (unsigned char)(src[src_start + i] & 0x0Fu);

            if (color != transparent_color) {
                draw_mask |= bit;

                if (color & 0x01u) blue_bits |= bit;
                if (color & 0x02u) red_bits |= bit;
                if (color & 0x04u) green_bits |= bit;
                if (color & 0x08u) intens_bits |= bit;
            }

            bit >>= 1;
            ++i;
        }

        if (draw_mask != 0) {
            GRAPH98_VRAM_BLUE[offset] =
                (uint8_t)((GRAPH98_VRAM_BLUE[offset] & (uint8_t)~draw_mask) | blue_bits);

            GRAPH98_VRAM_RED[offset] =
                (uint8_t)((GRAPH98_VRAM_RED[offset] & (uint8_t)~draw_mask) | red_bits);

            GRAPH98_VRAM_GREEN[offset] =
                (uint8_t)((GRAPH98_VRAM_GREEN[offset] & (uint8_t)~draw_mask) | green_bits);

            GRAPH98_VRAM_INTENS[offset] =
                (uint8_t)((GRAPH98_VRAM_INTENS[offset] & (uint8_t)~draw_mask) | intens_bits);
        }
    }
}


void graph98_hline(int x0, int x1, int y, unsigned char color)
{
    int x;
    int tmp;

    if (y < 0 || y >= GRAPH98_HEIGHT) {
        return;
    }

    if (x0 > x1) {
        tmp = x0;
        x0 = x1;
        x1 = tmp;
    }

    if (x1 < 0 || x0 >= GRAPH98_WIDTH) {
        return;
    }

    if (x0 < 0) {
        x0 = 0;
    }
    if (x1 >= GRAPH98_WIDTH) {
        x1 = GRAPH98_WIDTH - 1;
    }

    for (x = x0; x <= x1; ++x) {
        graph98_pset(x, y, color);
    }
}

void graph98_vline(int x, int y0, int y1, unsigned char color)
{
    int y;
    int tmp;

    if (x < 0 || x >= GRAPH98_WIDTH) {
        return;
    }

    if (y0 > y1) {
        tmp = y0;
        y0 = y1;
        y1 = tmp;
    }

    if (y1 < 0 || y0 >= GRAPH98_HEIGHT) {
        return;
    }

    if (y0 < 0) {
        y0 = 0;
    }
    if (y1 >= GRAPH98_HEIGHT) {
        y1 = GRAPH98_HEIGHT - 1;
    }

    for (y = y0; y <= y1; ++y) {
        graph98_pset(x, y, color);
    }
}

void graph98_rect(int x0, int y0, int x1, int y1, unsigned char color)
{
    int tmp;

    if (x0 > x1) {
        tmp = x0;
        x0 = x1;
        x1 = tmp;
    }

    if (y0 > y1) {
        tmp = y0;
        y0 = y1;
        y1 = tmp;
    }

    graph98_hline(x0, x1, y0, color);
    graph98_hline(x0, x1, y1, color);
    graph98_vline(x0, y0, y1, color);
    graph98_vline(x1, y0, y1, color);
}

static void graph98_fill_byte_rect(int byte_x0, int byte_x1,
                                      int y0, int y1,
                                      uint8_t blue_value,
                                      uint8_t red_value,
                                      uint8_t green_value,
                                      uint8_t intens_value)
{
    int y;

    for (y = y0; y <= y1; ++y) {
        uint16_t offset;
        int bx;

        offset = (uint16_t)(y * GRAPH98_BYTES_PER_LINE + byte_x0);
        for (bx = byte_x0; bx <= byte_x1; ++bx) {
            uint16_t p;

            p = (uint16_t)(offset + (bx - byte_x0));
            GRAPH98_VRAM_BLUE[p] = blue_value;
            GRAPH98_VRAM_RED[p] = red_value;
            GRAPH98_VRAM_GREEN[p] = green_value;
            GRAPH98_VRAM_INTENS[p] = intens_value;
        }
    }
}

void graph98_boxfill(int x0, int y0, int x1, int y1, unsigned char color)
{
    int tmp;
    int byte_x0;
    int byte_x1;
    int left_aligned_x;
    int right_aligned_x;
    uint8_t blue_value;
    uint8_t red_value;
    uint8_t green_value;
    uint8_t intens_value;

    if (x0 > x1) {
        tmp = x0;
        x0 = x1;
        x1 = tmp;
    }

    if (y0 > y1) {
        tmp = y0;
        y0 = y1;
        y1 = tmp;
    }

    if (x1 < 0 || y1 < 0 || x0 >= GRAPH98_WIDTH || y0 >= GRAPH98_HEIGHT) {
        return;
    }

    if (x0 < 0) {
        x0 = 0;
    }
    if (x1 >= GRAPH98_WIDTH) {
        x1 = GRAPH98_WIDTH - 1;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (y1 >= GRAPH98_HEIGHT) {
        y1 = GRAPH98_HEIGHT - 1;
    }

    color &= 0x0Fu;

#if USE_EGC
    {
        uint16_t width;
        uint16_t height;

        width = (uint16_t)(x1 - x0 + 1);
        height = (uint16_t)(y1 - y0 + 1);

        if (width != 0u && height != 0u &&
            x0 >= 0 && y0 >= 0 &&
            x1 < GRAPH98_WIDTH && y1 < GRAPH98_HEIGHT &&
            (x0 & 15) == 0 && (width & 15u) == 0u &&
            egc98_bios_flag_present() &&
            egc98_boxfill_aligned16((uint16_t)x0, (uint16_t)y0,
                                    width, height, color) == EGC98_DRAW_OK) {
            return;
        }
    }
#endif

    /*
     * PC-98 の 16 色画面は 8 ドットで 1 バイトです。
     * 左右端が 8 ドット境界に乗っている矩形は、graph98_pset() を通さず
     * VRAMへ1バイトずつ直接書くとかなり軽くなります。
     */
    if ((x0 & 7) == 0 && (x1 & 7) == 7) {
        byte_x0 = x0 >> 3;
        byte_x1 = x1 >> 3;

        blue_value  = (color & 0x01u) ? 0xFFu : 0x00u;
        red_value   = (color & 0x02u) ? 0xFFu : 0x00u;
        green_value = (color & 0x04u) ? 0xFFu : 0x00u;
        intens_value = (color & 0x08u) ? 0xFFu : 0x00u;

        graph98_fill_byte_rect(byte_x0, byte_x1, y0, y1,
                               blue_value, red_value,
                               green_value, intens_value);
        return;
    }

    /*
     * 左右端だけ従来通りドット描画し、中央の8ドット境界部分だけ高速化します。
     * 表示結果は従来の boxfill と同じです。
     */
    left_aligned_x = (x0 + 7) & ~7;
    right_aligned_x = (x1 + 1) & ~7;

    if (left_aligned_x <= right_aligned_x - 1) {
        int y;

        if (x0 < left_aligned_x) {
            for (y = y0; y <= y1; ++y) {
                graph98_hline(x0, left_aligned_x - 1, y, color);
            }
        }

        byte_x0 = left_aligned_x >> 3;
        byte_x1 = (right_aligned_x >> 3) - 1;

        blue_value  = (color & 0x01u) ? 0xFFu : 0x00u;
        red_value   = (color & 0x02u) ? 0xFFu : 0x00u;
        green_value = (color & 0x04u) ? 0xFFu : 0x00u;
        intens_value = (color & 0x08u) ? 0xFFu : 0x00u;

        graph98_fill_byte_rect(byte_x0, byte_x1, y0, y1,
                               blue_value, red_value,
                               green_value, intens_value);

        if (right_aligned_x <= x1) {
            for (y = y0; y <= y1; ++y) {
                graph98_hline(right_aligned_x, x1, y, color);
            }
        }
        return;
    }

    for (tmp = y0; tmp <= y1; ++tmp) {
        graph98_hline(x0, x1, tmp, color);
    }
}

void graph98_clear(unsigned char color)
{
    graph98_boxfill(0, 0, GRAPH98_WIDTH - 1, GRAPH98_HEIGHT - 1, color);
}

static int graph98_read_g98_header(FILE *fp, struct graph98_g98_header *header)
{
    uint8_t raw[13];

    if (fread(raw, 1u, 13u, fp) != 13u) {
        return 0;
    }

    header->magic[0] = raw[0];
    header->magic[1] = raw[1];
    header->magic[2] = raw[2];
    header->magic[3] = raw[3];

    header->width  = (uint16_t)raw[4] | ((uint16_t)raw[5] << 8);
    header->height = (uint16_t)raw[6] | ((uint16_t)raw[7] << 8);

    header->version = raw[8];

    header->plane_order[0] = raw[9];
    header->plane_order[1] = raw[10];
    header->plane_order[2] = raw[11];
    header->plane_order[3] = raw[12];

    return 1;
}

static int __attribute__((noinline, optimize("Os")))
graph98_is_valid_g98_header(const struct graph98_g98_header *header)
{
    return header->magic[0] == GRAPH98_G98_MAGIC_0 &&
           header->magic[1] == GRAPH98_G98_MAGIC_1 &&
           header->magic[2] == GRAPH98_G98_MAGIC_2 &&
           header->magic[3] == GRAPH98_G98_MAGIC_3 &&
           header->width == GRAPH98_WIDTH &&
           header->height == GRAPH98_HEIGHT &&
           header->version == GRAPH98_G98_VERSION &&
           header->plane_order[0] == 'B' &&
           header->plane_order[1] == 'R' &&
           header->plane_order[2] == 'G' &&
           header->plane_order[3] == 'I';
}

static int graph98_read_sprite_header(FILE *fp,
                                      struct graph98_sprite_header *header)
{
    uint8_t raw[9];

    if (fread(raw, 1u, 9u, fp) != 9u) {
        return 0;
    }

    header->magic[0] = raw[0];
    header->magic[1] = raw[1];
    header->magic[2] = raw[2];
    header->magic[3] = raw[3];
    header->width = (uint16_t)raw[4] | ((uint16_t)raw[5] << 8);
    header->height = (uint16_t)raw[6] | ((uint16_t)raw[7] << 8);
    header->version = raw[8];

    return 1;
}

void graph98_wait_vsync(void)
{
    uint16_t guard;

    guard = 0;

    while ((io_in8(GDC_MASTER_STATUS) & GDC_STATUS_VSYNC) != 0u) {

        if (++guard == 0u) {
            return;
        }
    }

    guard = 0;

    while ((io_in8(GDC_MASTER_STATUS) & GDC_STATUS_VSYNC) == 0u) {

        if (++guard == 0u) {
            return;
        }
    }
}


int graph98_load_g98(const char *path)
{
    struct graph98_g98_header header;
    FILE *fp;
    int ok;

    fp = fopen(path, "rb");
    if (fp == 0) {
        return 0;
    }

    ok = 0;

    //if (fread(&header, 1u, sizeof(header), fp) != sizeof(header)) {
    //    fclose(fp);
    //    return 0;
    //}

    if (!graph98_read_g98_header(fp, &header)) {
        fclose(fp);
        return 0;
    }





    if (!graph98_is_valid_g98_header(&header)) {
        fclose(fp);
        return 0;
    }

    if (!graph98_copy_plane_from_file(fp, GRAPH98_VRAM_BLUE)) {
        fclose(fp);
        return 0;
    }
    if (!graph98_copy_plane_from_file(fp, GRAPH98_VRAM_RED)) {
        fclose(fp);
        return 0;
    }
    if (!graph98_copy_plane_from_file(fp, GRAPH98_VRAM_GREEN)) {
        fclose(fp);
        return 0;
    }
    if (!graph98_copy_plane_from_file(fp, GRAPH98_VRAM_INTENS)) {
        fclose(fp);
        return 0;
    }

    ok = 1;
    fclose(fp);
    return ok;
}

static int graph98_copy_plane_rect_from_file(FILE *fp,
                                             volatile uint8_t __far *plane,
                                             unsigned long plane_start,
                                             int x0,
                                             int y0,
                                             int x1,
                                             int y1)
{
    int y;
    int byte_x0;
    int byte_x1;
    int bytes;
    int i;
    unsigned long pos;

    byte_x0 = x0 >> 3;
    byte_x1 = x1 >> 3;
    bytes = byte_x1 - byte_x0 + 1;

    if (bytes <= 0 || bytes > GRAPH98_BYTES_PER_LINE) {
        return 0;
    }

    /*
     * 旧版は 1 行ごとに fseek していました。
     * PC-98 の DOS 環境では fseek の回数が重いので、各プレーンで最初に1回だけ seek し、
     * 以後は 80 バイトずつ順番に読んで必要な範囲だけ VRAM へ戻します。
     */
    pos = plane_start + (unsigned long)y0 * GRAPH98_BYTES_PER_LINE;
    if (fseek(fp, pos, SEEK_SET) != 0) {
        return 0;
    }

    for (y = y0; y <= y1; ++y) {
        uint16_t dst_offset;

        if (fread(graph98_image_work, 1u, GRAPH98_BYTES_PER_LINE, fp) !=
            GRAPH98_BYTES_PER_LINE) {
            return 0;
        }

        dst_offset = (uint16_t)(y * GRAPH98_BYTES_PER_LINE + byte_x0);
        for (i = 0; i < bytes; ++i) {
            plane[(uint16_t)(dst_offset + i)] =
                graph98_image_work[byte_x0 + i];
        }
    }

    return 1;
}

int graph98_load_g98_rect(const char *path, int x0, int y0, int x1, int y1)
{
    struct graph98_g98_header header;
    FILE *fp;
    unsigned long header_size;
    unsigned long plane_size;
    int ok;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= GRAPH98_WIDTH) x1 = GRAPH98_WIDTH - 1;
    if (y1 >= GRAPH98_HEIGHT) y1 = GRAPH98_HEIGHT - 1;

    if (x0 > x1 || y0 > y1) {
        return 0;
    }

    fp = fopen(path, "rb");
    if (fp == 0) {
        return 0;
    }

    if (!graph98_read_g98_header(fp, &header)) {
        fclose(fp);
        return 0;
    }

    if (header.magic[0] != GRAPH98_G98_MAGIC_0 ||
        header.magic[1] != GRAPH98_G98_MAGIC_1 ||
        header.magic[2] != GRAPH98_G98_MAGIC_2 ||
        header.magic[3] != GRAPH98_G98_MAGIC_3 ||
        header.width != GRAPH98_WIDTH ||
        header.height != GRAPH98_HEIGHT ||
        header.version != GRAPH98_G98_VERSION) {
        fclose(fp);
        return 0;
    }

    header_size = 13UL;
    plane_size = (unsigned long)GRAPH98_BYTES_PER_LINE * GRAPH98_HEIGHT;

    ok = 1;

    if (!graph98_copy_plane_rect_from_file(fp, GRAPH98_VRAM_BLUE,
                                           header_size + plane_size * 0UL,
                                           x0, y0, x1, y1)) {
        ok = 0;
    }

    if (ok && !graph98_copy_plane_rect_from_file(fp, GRAPH98_VRAM_RED,
                                                 header_size + plane_size * 1UL,
                                                 x0, y0, x1, y1)) {
        ok = 0;
    }

    if (ok && !graph98_copy_plane_rect_from_file(fp, GRAPH98_VRAM_GREEN,
                                                 header_size + plane_size * 2UL,
                                                 x0, y0, x1, y1)) {
        ok = 0;
    }

    if (ok && !graph98_copy_plane_rect_from_file(fp, GRAPH98_VRAM_INTENS,
                                                 header_size + plane_size * 3UL,
                                                 x0, y0, x1, y1)) {
        ok = 0;
    }

    fclose(fp);
    return ok;
}

static int __attribute__((noinline, optimize("Os")))
graph98_copy_plane_interlace_range_from_file(
    FILE *fp,
    volatile uint8_t __far *plane,
    unsigned long plane_start,
    uint16_t start_y,
    uint16_t end_y,
    int reverse)
{
    uint16_t lines;
    unsigned long pos;
    int line;
    int line_end;
    int line_step;

    if (end_y <= start_y) {
        return 1;
    }

    lines = (uint16_t)(end_y - start_y);
    if (lines > GRAPH98_G98_INTERLACE_GROUP_LINES) {
        return 0;
    }

    pos = plane_start + (unsigned long)start_y * GRAPH98_BYTES_PER_LINE;
    if (fseek(fp, pos, SEEK_SET) != 0) {
        return 0;
    }

    if (fread(graph98_image_work, GRAPH98_BYTES_PER_LINE, lines, fp) != lines) {
        return 0;
    }

    /*
     * 画面高、帯幅、移動量はすべて8の倍数なので、各範囲は偶数Yから
     * 始まり偶数ライン数になる。正順の0,2,...が画面の偶数ライン、
     * 逆順の末尾,末尾-2,...が画面の奇数ラインになる。
     */
    if (reverse) {
        line = (int)lines - (int)GRAPH98_G98_INTERLACE_BOTTOM_OFFSET;
        line_end = -(int)GRAPH98_G98_INTERLACE_BOTTOM_OFFSET;
        line_step = -(int)GRAPH98_G98_INTERLACE_PERIOD;
    } else {
        line = 0;
        line_end = (int)lines;
        line_step = (int)GRAPH98_G98_INTERLACE_PERIOD;
    }

    while (line != line_end) {
        uint16_t src_y;
        uint16_t src_offset;
        uint16_t dst_offset;
        uint16_t i;

        src_y = (uint16_t)(start_y + (uint16_t)line);
        src_offset = (uint16_t)((uint16_t)line *
                                GRAPH98_BYTES_PER_LINE);
        dst_offset = (uint16_t)(src_y * GRAPH98_BYTES_PER_LINE);
        for (i = 0; i < GRAPH98_BYTES_PER_LINE; ++i) {
            plane[(uint16_t)(dst_offset + i)] =
                graph98_image_work[(uint16_t)(src_offset + i)];
        }
        line += line_step;
    }

    return 1;
}

int __attribute__((optimize("Os")))
graph98_load_g98_interlace(const char *path)
{
    struct graph98_g98_header header;
    FILE *fp;
    unsigned long header_size;
    unsigned long plane_size;
    uint16_t sweep_y;
    int plane_index;
    volatile uint8_t __far *planes[4];

    fp = fopen(path, "rb");
    if (fp == 0) {
        return 0;
    }

    if (!graph98_read_g98_header(fp, &header)) {
        fclose(fp);
        return 0;
    }

    if (!graph98_is_valid_g98_header(&header)) {
        fclose(fp);
        return 0;
    }

    planes[0] = GRAPH98_VRAM_BLUE;
    planes[1] = GRAPH98_VRAM_RED;
    planes[2] = GRAPH98_VRAM_GREEN;
    planes[3] = GRAPH98_VRAM_INTENS;

    header_size = 13UL;
    plane_size = (unsigned long)GRAPH98_BYTES_PER_LINE * GRAPH98_HEIGHT;
    sweep_y = 0;

    while (sweep_y <= GRAPH98_HEIGHT -
                      GRAPH98_G98_INTERLACE_GROUP_LINES) {
        uint16_t top_start;
        uint16_t top_end;
        uint16_t bottom_start;
        uint16_t bottom_end;

        top_end = (uint16_t)(sweep_y + GRAPH98_G98_INTERLACE_GROUP_LINES);
        bottom_start = (uint16_t)(GRAPH98_HEIGHT - top_end);
        if (sweep_y == 0) {
            top_start = 0;
            bottom_end = GRAPH98_HEIGHT;
        } else {
            top_start = (uint16_t)(top_end -
                                   GRAPH98_G98_INTERLACE_SWEEP_LINES);
            bottom_end = (uint16_t)(bottom_start +
                                    GRAPH98_G98_INTERLACE_SWEEP_LINES);
        }

        for (plane_index = 0; plane_index < 4; ++plane_index) {
            unsigned long plane_start;

            plane_start = header_size +
                          plane_size * (unsigned long)plane_index;

            if (!graph98_copy_plane_interlace_range_from_file(
                    fp,
                    planes[plane_index],
                    plane_start,
                    top_start,
                    top_end,
                    0)) {
                fclose(fp);
                return 0;
            }

            if (!graph98_copy_plane_interlace_range_from_file(
                    fp,
                    planes[plane_index],
                    plane_start,
                    bottom_start,
                    bottom_end,
                    1)) {
                fclose(fp);
                return 0;
            }
        }

        graph98_wait_vsync();
        sweep_y = (uint16_t)(sweep_y +
                             GRAPH98_G98_INTERLACE_SWEEP_LINES);
    }

    fclose(fp);
    return 1;
}


int graph98_draw_sprite_file_trans(const char *path, int x, int y,
                                   unsigned char transparent_color)
{
    struct graph98_sprite_header header;
    FILE *fp;
    uint16_t src_y;

    fp = fopen(path, "rb");
    if (fp == 0) {
        return 0;
    }

    if (!graph98_read_sprite_header(fp, &header)) {
        fclose(fp);
        return 0;
    }

    if (header.magic[0] != GRAPH98_SPRITE_MAGIC_0 ||
        header.magic[1] != GRAPH98_SPRITE_MAGIC_1 ||
        header.magic[2] != GRAPH98_SPRITE_MAGIC_2 ||
        header.magic[3] != GRAPH98_SPRITE_MAGIC_3) {
        fclose(fp);
        return 0;
    }

    if (header.version != GRAPH98_SPRITE_VERSION) {
        fclose(fp);
        return 0;
    }

    if (header.width == 0 || header.height == 0) {
        fclose(fp);
        return 0;
    }

    if (header.width > GRAPH98_SPRITE_MAX_WIDTH) {
        fclose(fp);
        return 0;
    }

    transparent_color &= 0x0Fu;

    src_y = 0;
    while (src_y < header.height) {
        uint16_t chunk_lines;
        uint16_t line;

        chunk_lines = (uint16_t)(header.height - src_y);
        if (chunk_lines > GRAPH98_SPRITE_CHUNK_LINES) {
            chunk_lines = GRAPH98_SPRITE_CHUNK_LINES;
        }

        if (fread(graph98_image_work, header.width, chunk_lines, fp) !=
            chunk_lines) {
            fclose(fp);
            return 0;
        }

        for (line = 0; line < chunk_lines; ++line) {
            uint16_t src_x;
            int dst_y;
            uint8_t *row;

            dst_y = y + (int)src_y + (int)line;
            if (dst_y < 0 || dst_y >= GRAPH98_HEIGHT) {
                continue;
            }

            row = graph98_image_work + (uint16_t)(header.width * line);

            {
                int src_start;
                int draw_width;
                int draw_x;

                src_start = 0;
                draw_width = (int)header.width;
                draw_x = x;

                if (draw_x < 0) {
                    src_start = -draw_x;
                    draw_width -= src_start;
                    draw_x = 0;
                }

                if (draw_x + draw_width > GRAPH98_WIDTH) {
                    draw_width = GRAPH98_WIDTH - draw_x;
                }

                if (draw_width > 0) {
                    graph98_draw_sprite_line_trans_fast(row,
                                                src_start,
                                                draw_width,
                                                draw_x,
                                                dst_y,
                                                transparent_color);
                }
            }
        }

        src_y = (uint16_t)(src_y + chunk_lines);
    }

    fclose(fp);
    return 1;
}

static void graph98_draw_sprite_buffer_line_trans(const uint8_t *row,
                                                  const struct graph98_sprite_header *header,
                                                  int x,
                                                  int y,
                                                  uint16_t src_y,
                                                  unsigned char transparent_color)
{
    int dst_y;
    int src_start;
    int draw_width;
    int draw_x;

    dst_y = y + (int)src_y;
    if (dst_y < 0 || dst_y >= GRAPH98_HEIGHT) {
        return;
    }

    src_start = 0;
    draw_width = (int)header->width;
    draw_x = x;

    if (draw_x < 0) {
        src_start = -draw_x;
        draw_width -= src_start;
        draw_x = 0;
    }

    if (draw_x + draw_width > GRAPH98_WIDTH) {
        draw_width = GRAPH98_WIDTH - draw_x;
    }

    if (draw_width > 0) {
        graph98_draw_sprite_line_trans_fast(row,
                                            src_start,
                                            draw_width,
                                            draw_x,
                                            dst_y,
                                            transparent_color);
    }
}

static int graph98_draw_sprite_file_trans_interlace_range(FILE *fp,
                                                          const struct graph98_sprite_header *header,
                                                          uint8_t *buffer,
                                                          const char *path,
                                                          int x,
                                                          int y,
                                                          uint16_t start_y,
                                                          uint16_t end_y,
                                                          uint16_t phase,
                                                          int reverse,
                                                          unsigned char transparent_color)
{
    uint16_t lines;
    unsigned long pos;

    if (end_y <= start_y) {
        return 1;
    }

    lines = (uint16_t)(end_y - start_y);
    pos = 9UL + (unsigned long)start_y * (unsigned long)header->width;
    if (fseek(fp, pos, SEEK_SET) != 0) {
        debug_log("interlace fseek failed: %s start_y=%u pos=%lu",
                  path, (unsigned)start_y, pos);
        return 0;
    }

    if (fread(buffer, header->width, lines, fp) != lines) {
        debug_log("interlace fread failed: %s start_y=%u lines=%u width=%u",
                  path,
                  (unsigned)start_y,
                  (unsigned)lines,
                  (unsigned)header->width);
        return 0;
    }

    if (reverse) {
        int line;

        for (line = (int)lines - 1; line >= 0; --line) {
            uint16_t src_y;
            uint8_t *row;

            src_y = (uint16_t)(start_y + (uint16_t)line);
            if ((src_y % GRAPH98_SPRITE_INTERLACE_PERIOD) != phase) {
                continue;
            }

            row = buffer + (uint16_t)(header->width * (uint16_t)line);
            graph98_draw_sprite_buffer_line_trans(row,
                                                  header,
                                                  x,
                                                  y,
                                                  src_y,
                                                  transparent_color);
        }
    } else {
        uint16_t line;

        for (line = 0; line < lines; ++line) {
            uint16_t src_y;
            uint8_t *row;

            src_y = (uint16_t)(start_y + line);
            if ((src_y % GRAPH98_SPRITE_INTERLACE_PERIOD) != phase) {
                continue;
            }

            row = buffer + (uint16_t)(header->width * line);
            graph98_draw_sprite_buffer_line_trans(row,
                                                  header,
                                                  x,
                                                  y,
                                                  src_y,
                                                  transparent_color);
        }
    }

    return 1;
}

int __attribute__((optimize("Os")))
graph98_draw_sprite_file_trans_interlace(const char *path, int x, int y,
                                         unsigned char transparent_color)
{
    struct graph98_sprite_header header;
    FILE *fp;
    uint16_t sweep_y;

    fp = fopen(path, "rb");
    if (fp == 0) {
        debug_log("interlace fopen failed: %s", path);
        return 0;
    }

    if (!graph98_read_sprite_header(fp, &header)) {
        debug_log("interlace header read failed: %s", path);
        fclose(fp);
        return 0;
    }

    if (header.magic[0] != GRAPH98_SPRITE_MAGIC_0 ||
        header.magic[1] != GRAPH98_SPRITE_MAGIC_1 ||
        header.magic[2] != GRAPH98_SPRITE_MAGIC_2 ||
        header.magic[3] != GRAPH98_SPRITE_MAGIC_3) {
        debug_log("interlace header magic invalid: %s %u %u %u %u",
                  path,
                  (unsigned)header.magic[0],
                  (unsigned)header.magic[1],
                  (unsigned)header.magic[2],
                  (unsigned)header.magic[3]);
        fclose(fp);
        return 0;
    }

    if (header.version != GRAPH98_SPRITE_VERSION) {
        debug_log("interlace header version invalid: %s version=%u",
                  path, (unsigned)header.version);
        fclose(fp);
        return 0;
    }

    if (header.width == 0 ||
        header.height == 0 ||
        header.width > GRAPH98_SPRITE_MAX_WIDTH ||
        header.height > GRAPH98_SPRITE_INTERLACE_MAX_HEIGHT) {
        debug_log("interlace sprite size invalid: %s width=%u height=%u",
                  path, (unsigned)header.width, (unsigned)header.height);
        fclose(fp);
        return 0;
    }

    transparent_color &= 0x0Fu;

    sweep_y = 0;
    while (sweep_y < header.height) {
        uint16_t top_start;
        uint16_t top_end;
        uint16_t bottom_start;
        uint16_t bottom_end;

        top_end = (uint16_t)(sweep_y + GRAPH98_SPRITE_INTERLACE_GROUP_LINES);
        if (top_end > header.height) {
            top_end = header.height;
        }

        if (sweep_y == 0) {
            top_start = 0;
        } else {
            top_start = (uint16_t)(sweep_y +
                                   GRAPH98_SPRITE_INTERLACE_GROUP_LINES -
                                   GRAPH98_SPRITE_INTERLACE_SWEEP_LINES);
            if (top_start > header.height) {
                top_start = header.height;
            }
        }
        bottom_start = (uint16_t)(header.height - top_end);
        bottom_end = (uint16_t)(header.height - top_start);

        if (!graph98_draw_sprite_file_trans_interlace_range(fp,
                                                            &header,
                                                            graph98_image_work,
                                                            path,
                                                            x,
                                                            y,
                                                            top_start,
                                                            top_end,
                                                            0u,
                                                            0,
                                                            transparent_color)) {
            fclose(fp);
            return 0;
        }

        if (!graph98_draw_sprite_file_trans_interlace_range(
                fp,
                &header,
                graph98_image_work,
                path,
                x,
                y,
                bottom_start,
                bottom_end,
                GRAPH98_SPRITE_INTERLACE_BOTTOM_OFFSET,
                1,
                transparent_color)) {
            fclose(fp);
            return 0;
        }

        graph98_wait_vsync();
        if (top_end == header.height && bottom_start == 0) {
            break;
        }
        sweep_y = (uint16_t)(sweep_y + GRAPH98_SPRITE_INTERLACE_SWEEP_LINES);
    }

    fclose(fp);

    return 1;
}


void graph98_draw_digit(int x, int y, int digit, unsigned char color)
{
    int row;
    int col;
    uint8_t bits;

    /*
     * 最小版なので、描けるのは 0〜9 だけです。
     * 範囲外なら何も描かずに戻ります。
     */
    if (digit < 0 || digit > 9) {
        return;
    }

    /*
     * 7 行を上から順番に見ていきます。
     * 各行には 5 ドットぶんの点灯パターンが入っています。
     */
    for (row = 0; row < 7; ++row) {
        bits = graph98_digit_font[digit][row];

        /*
         * 左から右へ 5 ドット確認します。
         * ビットが立っている場所だけ graph98_pset() で描きます。
         */
        for (col = 0; col < 5; ++col) {
            if (bits & (uint8_t)(1u << (4 - col))) {
                graph98_pset(x + col, y + row, color);
            }
        }
    }
}

void graph98_draw_number(int x, int y, int value, unsigned char color)
{
    int tens;
    int ones;

    /*
     * 今回は 0〜99 くらいまで描ければ十分です。
     * 負数は扱わないので、そのまま戻ります。
     */
    if (value < 0) {
        return;
    }

    /*
     * 1 桁ならそのまま描きます。
     */
    if (value < 10) {
        graph98_draw_digit(x, y, value, color);
        return;
    }

    /*
     * 2 桁は十の位と一の位に分けて描きます。
     * 100 以上が来ても、今回の用途では下 2 桁で十分です。
     */
    tens = (value / 10) % 10;
    ones = value % 10;

    graph98_draw_digit(x, y, tens, color);

    /*
     * 5 ドット幅の数字の後ろに、1 ドットだけ空白を入れます。
     */
    graph98_draw_digit(x + 6, y, ones, color);
}

void graph98_draw_char(int x, int y, char ch, unsigned char color)
{
    int row;
    int col;
    int letter_index;
    uint8_t bits;

    /*
     * スペースは「空白 1 文字」として扱います。
     * 何も描かず、そのまま戻ります。
     */
    if (ch == ' ') {
        return;
    }

    /*
     * 小文字は大文字にそろえます。
     * 今回は簡単のため、ASCII の範囲だけを見ます。
     */
    if (ch >= 'a' && ch <= 'z') {
        ch = (char)(ch - 'a' + 'A');
    }

    /*
     * 数字は、すでにある関数をそのまま使います。
     * これで数字用フォントを重複して持たずに済みます。
     */
    if (ch >= '0' && ch <= '9') {
        graph98_draw_digit(x, y, ch - '0', color);
        return;
    }

    /*
     * 英字は A〜Z だけ対応します。
     * それ以外の文字は今回は無視します。
     */
    if (ch < 'A' || ch > 'Z') {
        return;
    }

    letter_index = ch - 'A';

    /*
     * 7 行を上から順番に見ていきます。
     * 各行には 5 ドットぶんの点灯パターンが入っています。
     */
    for (row = 0; row < 7; ++row) {
        bits = alpha_font[letter_index][row];

        /*
         * 左から右へ 5 ドット確認します。
         * ビットが立っている場所だけ graph98_pset() で描きます。
         */
        for (col = 0; col < 5; ++col) {
            if (bits & (uint8_t)(1u << (4 - col))) {
                graph98_pset(x + col, y + row, color);
            }
        }
    }
}

void graph98_draw_string(int x, int y, const char *str, unsigned char color)
{
      int start_x;
      int line_count;

      if (str == 0) {
          return;
      }

      start_x = x;
      line_count = 1;

      while (*str != '\0') {
          if (*str == '\n') {
              ++line_count;
              if (line_count > 2) {
                  break;
              }
              x = start_x;
              y += 8;
              ++str;
              continue;
          }

          graph98_draw_char(x, y, *str, color);
          x += 6;
          ++str;
      }
}
