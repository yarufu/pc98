#include "graph98.h"

#include "debug.h"

#include <stdio.h>
#include <stdint.h>

#define GRAPH98_BYTES_PER_LINE 80u
#define GRAPH98_VRAM_PLANE_SIZE ((uint16_t)(GRAPH98_BYTES_PER_LINE * GRAPH98_HEIGHT))

#define GRAPH98_VRAM_BLUE   ((volatile uint8_t __far *)0xA8000000UL)
#define GRAPH98_VRAM_RED    ((volatile uint8_t __far *)0xB0000000UL)
#define GRAPH98_VRAM_GREEN  ((volatile uint8_t __far *)0xB8000000UL)
#define GRAPH98_VRAM_INTENS ((volatile uint8_t __far *)0xE0000000UL)

#define GRAPH98_PORT_COLOR_MODE 0x006A

#define GRAPH98_PORT_DISPLAY_PAGE 0x00A4
#define GRAPH98_PORT_ACCESS_PAGE  0x00A6
#define GRAPH98_PAGE_FRONT 0x00u
#define GRAPH98_PAGE_BACK  0x01u

#define GRAPH98_PORT_PALETTE_INDEX 0x00A8
#define GRAPH98_PORT_PALETTE_G     0x00AA
#define GRAPH98_PORT_PALETTE_R     0x00AC
#define GRAPH98_PORT_PALETTE_B     0x00AE

#define GRAPH98_G98_MAGIC_0 'G'
#define GRAPH98_G98_MAGIC_1 '9'
#define GRAPH98_G98_MAGIC_2 '8'
#define GRAPH98_G98_MAGIC_3 'B'
#define GRAPH98_G98_VERSION 1u
#define GRAPH98_G98_INTERLACE_PERIOD 2u
#define GRAPH98_G98_INTERLACE_BOTTOM_OFFSET 1u
#define GRAPH98_G98_INTERLACE_LINES_PER_SIDE 4u
#define GRAPH98_G98_INTERLACE_STAGE_LINES \
    (GRAPH98_G98_INTERLACE_LINES_PER_SIDE * 2u)
#define GRAPH98_G98_INTERLACE_PLANE_BYTES \
    (GRAPH98_BYTES_PER_LINE * GRAPH98_G98_INTERLACE_STAGE_LINES)
#define GRAPH98_G98_INTERLACE_STAGE_BYTES \
    (GRAPH98_G98_INTERLACE_PLANE_BYTES * 4u)
#define GRAPH98_G98_INTERLACE_STAGE_COUNT \
    (GRAPH98_HEIGHT / \
     (GRAPH98_G98_INTERLACE_LINES_PER_SIDE * GRAPH98_G98_INTERLACE_PERIOD))

#define GRAPH98_SPRITE_MAX_WIDTH 256u
#define GRAPH98_MONEY_SPRITE_WIDTH 160u
#define GRAPH98_MONEY_SPRITE_HEIGHT 16u
#define GRAPH98_MONEY_DIGIT_WIDTH 16u
#define GRAPH98_MONEY_DIGIT_COUNT 5u
#define GRAPH98_MONEY_SPRITE_BYTES \
    (GRAPH98_MONEY_SPRITE_WIDTH * GRAPH98_MONEY_SPRITE_HEIGHT)

#define GRAPH98_STAND_WIDTH 256u
#define GRAPH98_STAND_HEIGHT 290u
#define GRAPH98_STAND_INTERLACE_PERIOD 2u
#define GRAPH98_STAND_INTERLACE_BOTTOM_OFFSET 1u
#define GRAPH98_STAND_INTERLACE_GROUP_LINES 32u
#define GRAPH98_STAND_INTERLACE_SWEEP_LINES 8u
#define GRAPH98_STAND_BYTES_PER_LINE (GRAPH98_STAND_WIDTH / 8u)
#define GRAPH98_STAND_INTERLACE_PLANE_BYTES \
    (GRAPH98_STAND_BYTES_PER_LINE * \
     GRAPH98_STAND_INTERLACE_GROUP_LINES)
#define GRAPH98_STAND_INTERLACE_STAGE_BYTES \
    (GRAPH98_STAND_INTERLACE_PLANE_BYTES * 4u)
#define GRAPH98_STAND_INTERLACE_STAGE_COUNT \
    (1u + ((GRAPH98_STAND_HEIGHT - \
            GRAPH98_STAND_INTERLACE_GROUP_LINES + \
            GRAPH98_STAND_INTERLACE_SWEEP_LINES - 1u) / \
           GRAPH98_STAND_INTERLACE_SWEEP_LINES))

#define GRAPH98_STAND_TRANSFER_LINES 8u
#define GRAPH98_STAND_TRANSFER_PLANES 4u
#define GRAPH98_RECT_TRANSFER_LINES 8u
#define GRAPH98_RECT_TRANSFER_PLANES 4u
#define GRAPH98_RECT_TRANSFER_MAX_BYTES \
    (GRAPH98_BYTES_PER_LINE * GRAPH98_RECT_TRANSFER_LINES * \
     GRAPH98_RECT_TRANSFER_PLANES)

#define GRAPH98_IMAGE_WORK_SIZE 8192u
#define GRAPH98_G98_CHUNK_LINES \
    (GRAPH98_IMAGE_WORK_SIZE / GRAPH98_BYTES_PER_LINE)
#define GRAPH98_G98_RECT_CHUNK_LINES \
    (GRAPH98_IMAGE_WORK_SIZE / GRAPH98_BYTES_PER_LINE)
#define GRAPH98_STAND_TRANSFER_CHUNK_LINES \
    (GRAPH98_IMAGE_WORK_SIZE / \
     (GRAPH98_STAND_BYTES_PER_LINE * GRAPH98_STAND_TRANSFER_PLANES))

/* Image loaders are non-reentrant and share this buffer sequentially. */
static uint8_t graph98_image_work[GRAPH98_IMAGE_WORK_SIZE];

_Static_assert(GRAPH98_IMAGE_WORK_SIZE == 8192u,
               "image work buffer must be 8 KB");
_Static_assert(GRAPH98_VRAM_PLANE_SIZE == 32000u,
               "full-screen VRAM plane must contain 32,000 bytes");
_Static_assert(GRAPH98_IMAGE_WORK_SIZE >= GRAPH98_SPRITE_MAX_WIDTH,
               "sprite chunk must contain at least one line");
_Static_assert(GRAPH98_MONEY_SPRITE_BYTES <= GRAPH98_IMAGE_WORK_SIZE,
               "money sprite must fit image work buffer");
_Static_assert(GRAPH98_G98_CHUNK_LINES >= 1u,
               "G98 chunk must contain at least one line");
_Static_assert(GRAPH98_G98_RECT_CHUNK_LINES >= 1u,
               "G98 rect chunk must contain at least one line");
_Static_assert(GRAPH98_BYTES_PER_LINE * GRAPH98_G98_RECT_CHUNK_LINES <=
                   GRAPH98_IMAGE_WORK_SIZE,
               "G98 rect chunk exceeds image work buffer");
_Static_assert(GRAPH98_BYTES_PER_LINE * GRAPH98_G98_CHUNK_LINES <=
                   GRAPH98_IMAGE_WORK_SIZE,
               "G98 chunk exceeds image work buffer");
_Static_assert(GRAPH98_G98_CHUNK_LINES == 102u,
               "full-screen VRAM chunk must contain 102 lines");
_Static_assert(GRAPH98_BYTES_PER_LINE * GRAPH98_G98_CHUNK_LINES == 8160u,
               "full-screen VRAM chunk must contain 8,160 bytes");
_Static_assert(GRAPH98_BYTES_PER_LINE * GRAPH98_G98_CHUNK_LINES - 1u ==
                   8159u,
               "full-screen VRAM chunk maximum work index must be 8,159");
_Static_assert(GRAPH98_G98_INTERLACE_STAGE_LINES == 8u,
               "G98 interlace stage must contain eight lines");
_Static_assert(GRAPH98_G98_INTERLACE_STAGE_BYTES == 2560u,
               "G98 interlace stage must contain 2,560 bytes");
_Static_assert(GRAPH98_G98_INTERLACE_STAGE_BYTES <= GRAPH98_IMAGE_WORK_SIZE,
               "G98 interlace stage exceeds image work buffer");
_Static_assert(GRAPH98_G98_INTERLACE_STAGE_COUNT == 50u,
               "G98 interlace must complete in 50 stages");
_Static_assert((GRAPH98_STAND_WIDTH % 8u) == 0u,
               "stand width must be byte aligned");
_Static_assert(GRAPH98_STAND_BYTES_PER_LINE == 32u,
               "stand line must contain 32 bytes per plane");
_Static_assert(GRAPH98_STAND_INTERLACE_PLANE_BYTES == 1024u,
               "stand stage plane must contain 1,024 bytes");
_Static_assert(GRAPH98_STAND_INTERLACE_STAGE_BYTES == 4096u,
               "stand stage must contain 4,096 bytes");
_Static_assert(GRAPH98_STAND_INTERLACE_STAGE_BYTES <=
                   GRAPH98_IMAGE_WORK_SIZE,
               "stand stage exceeds image work buffer");
_Static_assert(GRAPH98_STAND_INTERLACE_STAGE_BYTES - 1u == 4095u,
               "stand stage maximum work index must be 4,095");
_Static_assert(GRAPH98_STAND_INTERLACE_STAGE_COUNT == 34u,
               "stand interlace must complete in 34 stages");
_Static_assert(GRAPH98_STAND_INTERLACE_PERIOD == 2u,
               "stand interlace period must be two");
_Static_assert(GRAPH98_STAND_INTERLACE_BOTTOM_OFFSET == 1u,
               "stand bottom phase must be odd");
_Static_assert(GRAPH98_STAND_INTERLACE_GROUP_LINES == 32u,
               "stand first stage must contain 32 lines");
_Static_assert(GRAPH98_STAND_INTERLACE_SWEEP_LINES == 8u,
               "stand normal stage must contain eight lines");
_Static_assert(GRAPH98_STAND_TRANSFER_LINES == 8u,
               "stand transfer unit must contain eight lines");
_Static_assert(GRAPH98_STAND_TRANSFER_CHUNK_LINES == 64u,
               "stand transfer chunk must contain 64 lines");
_Static_assert(GRAPH98_STAND_TRANSFER_CHUNK_LINES *
                   GRAPH98_STAND_BYTES_PER_LINE *
                   GRAPH98_STAND_TRANSFER_PLANES <= GRAPH98_IMAGE_WORK_SIZE,
               "stand transfer chunk exceeds image work buffer");
_Static_assert(GRAPH98_STAND_TRANSFER_CHUNK_LINES *
                   GRAPH98_STAND_BYTES_PER_LINE *
                   GRAPH98_STAND_TRANSFER_PLANES - 1u == 8191u,
               "stand transfer maximum work index must be 8,191");
_Static_assert(GRAPH98_RECT_TRANSFER_MAX_BYTES == 2560u,
               "full-width rect transfer unit must contain 2,560 bytes");
_Static_assert(GRAPH98_RECT_TRANSFER_MAX_BYTES <= GRAPH98_IMAGE_WORK_SIZE,
               "rect transfer unit exceeds image work buffer");

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
        : "cc", "memory");
}

void graph98_restore_default_pages(void)
{
    graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_FRONT);
    graph98_out8(GRAPH98_PORT_DISPLAY_PAGE, GRAPH98_PAGE_FRONT);
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
     * T98-NEXT、PC-9821V13、PC-9821Ra43で動作確認済み。
     * 標準640x400・16色以外のモードは対象外です。
     */
    graph98_set_mode_640x400_16color();
    graph98_restore_default_pages();
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

static int __attribute__((noinline, optimize("Os")))
graph98_is_valid_sprite_header(const struct graph98_sprite_header *header)
{
    return header->magic[0] == GRAPH98_SPRITE_MAGIC_0 &&
           header->magic[1] == GRAPH98_SPRITE_MAGIC_1 &&
           header->magic[2] == GRAPH98_SPRITE_MAGIC_2 &&
           header->magic[3] == GRAPH98_SPRITE_MAGIC_3 &&
           header->version == GRAPH98_SPRITE_VERSION &&
           header->width != 0 && header->height != 0 &&
           header->width <= GRAPH98_SPRITE_MAX_WIDTH;
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

static void __attribute__((noinline, optimize("Os")))
graph98_copy_vram_plane_to_front(volatile uint8_t __far *plane)
{
    uint16_t chunk_y;

    chunk_y = 0;
    while (chunk_y < GRAPH98_HEIGHT) {
        uint16_t chunk_lines;
        uint16_t byte_count;
        uint16_t vram_offset;
        uint16_t i;

        chunk_lines = (uint16_t)(GRAPH98_HEIGHT - chunk_y);
        if (chunk_lines > GRAPH98_G98_CHUNK_LINES) {
            chunk_lines = GRAPH98_G98_CHUNK_LINES;
        }
        byte_count = (uint16_t)(chunk_lines * GRAPH98_BYTES_PER_LINE);
        vram_offset = (uint16_t)(chunk_y * GRAPH98_BYTES_PER_LINE);

        graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_BACK);
        for (i = 0; i < byte_count; ++i) {
            graph98_image_work[i] = plane[(uint16_t)(vram_offset + i)];
        }

        graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_FRONT);
        for (i = 0; i < byte_count; ++i) {
            plane[(uint16_t)(vram_offset + i)] = graph98_image_work[i];
        }

        chunk_y = (uint16_t)(chunk_y + chunk_lines);
    }
}

int __attribute__((optimize("Os")))
graph98_draw_scene_file_trans_vram(
    const char *background_path,
    const char *left_sprite_path,
    const char *right_sprite_path,
    int left_x,
    int right_x,
    int stand_y,
    unsigned char transparent_color)
{
    int ok;

    ok = 0;

    /* Page 0 remains displayed until page 1 contains the complete scene. */
    graph98_out8(GRAPH98_PORT_DISPLAY_PAGE, GRAPH98_PAGE_FRONT);
    if (background_path == 0 || background_path[0] == '\0') {
        goto cleanup;
    }

    graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_BACK);
    if (!graph98_load_g98(background_path)) {
        goto cleanup;
    }
    if (left_sprite_path != 0 && left_sprite_path[0] != '\0' &&
        !graph98_draw_sprite_file_trans(
            left_sprite_path, left_x, stand_y, transparent_color)) {
        goto cleanup;
    }
    if (right_sprite_path != 0 && right_sprite_path[0] != '\0' &&
        !graph98_draw_sprite_file_trans(
            right_sprite_path, right_x, stand_y, transparent_color)) {
        goto cleanup;
    }

    /* No file I/O or transparency work occurs after this first VSYNC. */
    graph98_wait_vsync();
    graph98_out8(GRAPH98_PORT_DISPLAY_PAGE, GRAPH98_PAGE_BACK);

    graph98_copy_vram_plane_to_front(GRAPH98_VRAM_BLUE);
    graph98_copy_vram_plane_to_front(GRAPH98_VRAM_RED);
    graph98_copy_vram_plane_to_front(GRAPH98_VRAM_GREEN);
    graph98_copy_vram_plane_to_front(GRAPH98_VRAM_INTENS);

    graph98_wait_vsync();
    graph98_out8(GRAPH98_PORT_DISPLAY_PAGE, GRAPH98_PAGE_FRONT);
    ok = 1;

cleanup:
    graph98_restore_default_pages();
    return ok;
}

static void __attribute__((noinline, optimize("Os")))
graph98_transfer_rect_unit(
    uint16_t byte_x,
    uint16_t y,
    uint16_t bytes_per_row,
    uint16_t line_count,
    int write_to_vram)
{
    uint16_t plane_bytes;
    uint16_t plane_number;

    plane_bytes = (uint16_t)(bytes_per_row * line_count);

    for (plane_number = 0;
         plane_number < GRAPH98_RECT_TRANSFER_PLANES;
         ++plane_number) {
        volatile uint8_t __far *plane;
        uint16_t line;
        uint16_t work_start;

        if (plane_number == 0u) {
            plane = GRAPH98_VRAM_BLUE;
        } else if (plane_number == 1u) {
            plane = GRAPH98_VRAM_RED;
        } else if (plane_number == 2u) {
            plane = GRAPH98_VRAM_GREEN;
        } else {
            plane = GRAPH98_VRAM_INTENS;
        }
        work_start = (uint16_t)(plane_bytes * plane_number);

        for (line = 0; line < line_count; ++line) {
            uint16_t vram_offset;
            uint16_t work_offset;
            uint16_t i;

            vram_offset = (uint16_t)(
                (y + line) * GRAPH98_BYTES_PER_LINE + byte_x);
            work_offset = (uint16_t)(
                work_start + line * bytes_per_row);

            if (write_to_vram) {
                for (i = 0; i < bytes_per_row; ++i) {
                    plane[(uint16_t)(vram_offset + i)] =
                        graph98_image_work[(uint16_t)(work_offset + i)];
                }
            } else {
                for (i = 0; i < bytes_per_row; ++i) {
                    graph98_image_work[(uint16_t)(work_offset + i)] =
                        plane[(uint16_t)(vram_offset + i)];
                }
            }
        }
    }
}

int __attribute__((optimize("Os")))
graph98_prepare_rect_back_vram(int x0, int y0, int x1, int y1)
{
    uint16_t byte_x;
    uint16_t bytes_per_row;
    uint16_t y;

    graph98_out8(GRAPH98_PORT_DISPLAY_PAGE, GRAPH98_PAGE_FRONT);

    if (x0 < 0 || y0 < 0 || x0 > x1 || y0 > y1 ||
        x1 >= GRAPH98_WIDTH || y1 >= GRAPH98_HEIGHT) {
        graph98_restore_default_pages();
        return 0;
    }

    byte_x = (uint16_t)(x0 >> 3);
    bytes_per_row = (uint16_t)((x1 >> 3) - (x0 >> 3) + 1);
    y = (uint16_t)y0;

    while (y <= (uint16_t)y1) {
        uint16_t line_count;

        line_count = (uint16_t)((uint16_t)y1 - y + 1u);
        if (line_count > GRAPH98_RECT_TRANSFER_LINES) {
            line_count = GRAPH98_RECT_TRANSFER_LINES;
        }

        graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_FRONT);
        graph98_transfer_rect_unit(
            byte_x, y, bytes_per_row, line_count, 0);
        graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_BACK);
        graph98_transfer_rect_unit(
            byte_x, y, bytes_per_row, line_count, 1);

        y = (uint16_t)(y + line_count);
    }

    /* The caller now draws the completed message into page 1. */
    return 1;
}

int __attribute__((optimize("Os")))
graph98_present_rect_back_vram(int x0, int y0, int x1, int y1)
{
    uint16_t byte_x;
    uint16_t bytes_per_row;
    uint16_t y;
    int first_write;

    if (x0 < 0 || y0 < 0 || x0 > x1 || y0 > y1 ||
        x1 >= GRAPH98_WIDTH || y1 >= GRAPH98_HEIGHT) {
        graph98_restore_default_pages();
        return 0;
    }

    byte_x = (uint16_t)(x0 >> 3);
    bytes_per_row = (uint16_t)((x1 >> 3) - (x0 >> 3) + 1);
    y = (uint16_t)y0;
    first_write = 1;

    while (y <= (uint16_t)y1) {
        uint16_t line_count;

        line_count = (uint16_t)((uint16_t)y1 - y + 1u);
        if (line_count > GRAPH98_RECT_TRANSFER_LINES) {
            line_count = GRAPH98_RECT_TRANSFER_LINES;
        }

        graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_BACK);
        graph98_transfer_rect_unit(
            byte_x, y, bytes_per_row, line_count, 0);
        graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_FRONT);
        if (first_write) {
            graph98_wait_vsync();
            first_write = 0;
        }

        /* Complete B/R/G/I for each fixed eight-line unit. */
        graph98_transfer_rect_unit(
            byte_x, y, bytes_per_row, line_count, 1);

        y = (uint16_t)(y + line_count);
    }

    graph98_restore_default_pages();
    return 1;
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
     * 以後は複数ラインを一括で読んで必要な範囲だけ VRAM へ戻します。
     */
    pos = plane_start + (unsigned long)y0 * GRAPH98_BYTES_PER_LINE;
    if (fseek(fp, pos, SEEK_SET) != 0) {
        return 0;
    }

    y = y0;
    while (y <= y1) {
        uint16_t chunk_lines;
        uint16_t local_line;

        chunk_lines = (uint16_t)(y1 - y + 1);
        if (chunk_lines > GRAPH98_G98_RECT_CHUNK_LINES) {
            chunk_lines = GRAPH98_G98_RECT_CHUNK_LINES;
        }

        if (fread(graph98_image_work, GRAPH98_BYTES_PER_LINE,
                  chunk_lines, fp) != chunk_lines) {
            return 0;
        }

        for (local_line = 0; local_line < chunk_lines; ++local_line) {
            uint16_t dst_offset;
            uint16_t src_offset;

            dst_offset = (uint16_t)((y + (int)local_line) *
                                    GRAPH98_BYTES_PER_LINE + byte_x0);
            src_offset = (uint16_t)(local_line * GRAPH98_BYTES_PER_LINE +
                                    byte_x0);
            for (i = 0; i < bytes; ++i) {
                plane[(uint16_t)(dst_offset + i)] =
                    graph98_image_work[(uint16_t)(src_offset + i)];
            }
        }

        y += (int)chunk_lines;
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

static void __attribute__((noinline, optimize("Os")))
graph98_read_interlace_stage_plane(volatile uint8_t __far *plane,
                                   uint16_t top_y,
                                   uint16_t bottom_y,
                                   uint16_t work_start)
{
    uint16_t line_index;

    for (line_index = 0;
         line_index < GRAPH98_G98_INTERLACE_LINES_PER_SIDE;
         ++line_index) {
        uint16_t top_offset;
        uint16_t bottom_offset;
        uint16_t top_work;
        uint16_t bottom_work;
        uint16_t i;

        top_offset = (uint16_t)(
            (top_y + line_index * GRAPH98_G98_INTERLACE_PERIOD) *
            GRAPH98_BYTES_PER_LINE);
        bottom_offset = (uint16_t)(
            (bottom_y - line_index * GRAPH98_G98_INTERLACE_PERIOD) *
            GRAPH98_BYTES_PER_LINE);
        top_work = (uint16_t)(work_start +
                              line_index * GRAPH98_BYTES_PER_LINE);
        bottom_work = (uint16_t)(work_start +
                                 (GRAPH98_G98_INTERLACE_LINES_PER_SIDE +
                                  line_index) * GRAPH98_BYTES_PER_LINE);

        for (i = 0; i < GRAPH98_BYTES_PER_LINE; ++i) {
            graph98_image_work[(uint16_t)(top_work + i)] =
                plane[(uint16_t)(top_offset + i)];
            graph98_image_work[(uint16_t)(bottom_work + i)] =
                plane[(uint16_t)(bottom_offset + i)];
        }
    }
}

static void __attribute__((noinline, optimize("Os")))
graph98_write_interlace_stage_plane(volatile uint8_t __far *plane,
                                    uint16_t top_y,
                                    uint16_t bottom_y,
                                    uint16_t work_start)
{
    uint16_t line_index;

    for (line_index = 0;
         line_index < GRAPH98_G98_INTERLACE_LINES_PER_SIDE;
         ++line_index) {
        uint16_t top_offset;
        uint16_t bottom_offset;
        uint16_t top_work;
        uint16_t bottom_work;
        uint16_t i;

        top_offset = (uint16_t)(
            (top_y + line_index * GRAPH98_G98_INTERLACE_PERIOD) *
            GRAPH98_BYTES_PER_LINE);
        bottom_offset = (uint16_t)(
            (bottom_y - line_index * GRAPH98_G98_INTERLACE_PERIOD) *
            GRAPH98_BYTES_PER_LINE);
        top_work = (uint16_t)(work_start +
                              line_index * GRAPH98_BYTES_PER_LINE);
        bottom_work = (uint16_t)(work_start +
                                 (GRAPH98_G98_INTERLACE_LINES_PER_SIDE +
                                  line_index) * GRAPH98_BYTES_PER_LINE);

        for (i = 0; i < GRAPH98_BYTES_PER_LINE; ++i) {
            plane[(uint16_t)(top_offset + i)] =
                graph98_image_work[(uint16_t)(top_work + i)];
            plane[(uint16_t)(bottom_offset + i)] =
                graph98_image_work[(uint16_t)(bottom_work + i)];
        }
    }
}

int __attribute__((optimize("Os")))
graph98_load_g98_interlace(const char *path)
{
    uint16_t stage;
    int ok;

    ok = 0;

    if (path == 0 || path[0] == '\0') {
        goto cleanup;
    }

    /* The displayed page stays at page 0 for the whole effect. */
    graph98_out8(GRAPH98_PORT_DISPLAY_PAGE, GRAPH98_PAGE_FRONT);
    graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_BACK);

    /* All file I/O finishes while the complete background is built here. */
    if (!graph98_load_g98(path)) {
        goto cleanup;
    }

    graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_FRONT);

    for (stage = 0; stage < GRAPH98_G98_INTERLACE_STAGE_COUNT; ++stage) {
        uint16_t top_y;
        uint16_t bottom_y;

        top_y = (uint16_t)(
            stage * GRAPH98_G98_INTERLACE_LINES_PER_SIDE *
            GRAPH98_G98_INTERLACE_PERIOD);
        bottom_y = (uint16_t)(
            (GRAPH98_HEIGHT - GRAPH98_G98_INTERLACE_BOTTOM_OFFSET) -
            stage * GRAPH98_G98_INTERLACE_LINES_PER_SIDE *
            GRAPH98_G98_INTERLACE_PERIOD);

        graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_BACK);
        graph98_read_interlace_stage_plane(
            GRAPH98_VRAM_BLUE, top_y, bottom_y,
            GRAPH98_G98_INTERLACE_PLANE_BYTES * 0u);
        graph98_read_interlace_stage_plane(
            GRAPH98_VRAM_RED, top_y, bottom_y,
            GRAPH98_G98_INTERLACE_PLANE_BYTES * 1u);
        graph98_read_interlace_stage_plane(
            GRAPH98_VRAM_GREEN, top_y, bottom_y,
            GRAPH98_G98_INTERLACE_PLANE_BYTES * 2u);
        graph98_read_interlace_stage_plane(
            GRAPH98_VRAM_INTENS, top_y, bottom_y,
            GRAPH98_G98_INTERLACE_PLANE_BYTES * 3u);

        graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_FRONT);
        graph98_wait_vsync();

        graph98_write_interlace_stage_plane(
            GRAPH98_VRAM_BLUE, top_y, bottom_y,
            GRAPH98_G98_INTERLACE_PLANE_BYTES * 0u);
        graph98_write_interlace_stage_plane(
            GRAPH98_VRAM_RED, top_y, bottom_y,
            GRAPH98_G98_INTERLACE_PLANE_BYTES * 1u);
        graph98_write_interlace_stage_plane(
            GRAPH98_VRAM_GREEN, top_y, bottom_y,
            GRAPH98_G98_INTERLACE_PLANE_BYTES * 2u);
        graph98_write_interlace_stage_plane(
            GRAPH98_VRAM_INTENS, top_y, bottom_y,
            GRAPH98_G98_INTERLACE_PLANE_BYTES * 3u);
    }

    ok = 1;

cleanup:
    graph98_restore_default_pages();
    return ok;
}

static void __attribute__((noinline, optimize("Os")))
graph98_transfer_stand_interlace_line_plane(
    volatile uint8_t __far *plane,
    uint16_t byte_x,
    uint16_t base_y,
    uint16_t line,
    uint16_t work_offset,
    int write_to_vram)
{
    uint16_t vram_offset;
    uint16_t i;

    vram_offset = (uint16_t)((base_y + line) * GRAPH98_BYTES_PER_LINE +
                             byte_x);

    if (write_to_vram) {
        for (i = 0; i < GRAPH98_STAND_BYTES_PER_LINE; ++i) {
            plane[(uint16_t)(vram_offset + i)] =
                graph98_image_work[(uint16_t)(work_offset + i)];
        }
    } else {
        for (i = 0; i < GRAPH98_STAND_BYTES_PER_LINE; ++i) {
            graph98_image_work[(uint16_t)(work_offset + i)] =
                plane[(uint16_t)(vram_offset + i)];
        }
    }
}

static uint16_t __attribute__((noinline, optimize("Os")))
graph98_transfer_stand_interlace_stage_plane(
    volatile uint8_t __far *plane,
    uint16_t byte_x,
    uint16_t base_y,
    uint16_t top_start,
    uint16_t top_end,
    uint16_t bottom_start,
    uint16_t bottom_end,
    uint16_t work_start,
    int write_to_vram)
{
    uint16_t line;
    uint16_t work_line;
    int reverse_line;

    work_line = 0;

    for (line = top_start; line < top_end; ++line) {
        uint16_t work_offset;

        if ((line % GRAPH98_STAND_INTERLACE_PERIOD) != 0u) {
            continue;
        }

        work_offset = (uint16_t)(
            work_start + work_line * GRAPH98_STAND_BYTES_PER_LINE);
        graph98_transfer_stand_interlace_line_plane(
            plane, byte_x, base_y, line, work_offset, write_to_vram);
        ++work_line;
    }

    for (reverse_line = (int)bottom_end - 1;
         reverse_line >= (int)bottom_start;
         --reverse_line) {
        uint16_t work_offset;

        line = (uint16_t)reverse_line;
        if ((line % GRAPH98_STAND_INTERLACE_PERIOD) !=
            GRAPH98_STAND_INTERLACE_BOTTOM_OFFSET) {
            continue;
        }

        work_offset = (uint16_t)(
            work_start + work_line * GRAPH98_STAND_BYTES_PER_LINE);
        graph98_transfer_stand_interlace_line_plane(
            plane, byte_x, base_y, line, work_offset, write_to_vram);
        ++work_line;
    }

    return work_line;
}

int __attribute__((optimize("Os")))
graph98_draw_stand_file_trans_interlace(
    const char *background_path, const char *sprite_path,
    int x, int y, unsigned char transparent_color)
{
    uint16_t byte_x;
    uint16_t stage;
    int ok;

    ok = 0;

    /* The displayed page remains page 0, including during preparation. */
    graph98_out8(GRAPH98_PORT_DISPLAY_PAGE, GRAPH98_PAGE_FRONT);

    if (background_path == 0 || background_path[0] == '\0') {
        debug_log("stand interlace background missing: x=%d", x);
        goto cleanup;
    }

    if (x < 0 || y < 0 ||
        x + (int)GRAPH98_STAND_WIDTH > GRAPH98_WIDTH ||
        y + (int)GRAPH98_STAND_HEIGHT > GRAPH98_HEIGHT ||
        (x & 7) != 0) {
        debug_log("stand interlace rect invalid: x=%d y=%d", x, y);
        goto cleanup;
    }

    byte_x = (uint16_t)(x >> 3);
    graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_BACK);

    /* All G98 and SPR file I/O, and all transparency work, ends here. */
    if (!graph98_load_g98_rect(
            background_path,
            x, y,
            x + (int)GRAPH98_STAND_WIDTH - 1,
            y + (int)GRAPH98_STAND_HEIGHT - 1)) {
        debug_log("stand interlace background load failed: %s x=%d",
                  background_path, x);
        goto cleanup;
    }

    if (sprite_path != 0 && sprite_path[0] != '\0' &&
        !graph98_draw_sprite_file_trans(
            sprite_path, x, y, transparent_color)) {
        debug_log("stand interlace sprite load failed: %s x=%d",
                  sprite_path, x);
        goto cleanup;
    }

    graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_FRONT);

    for (stage = 0; stage < GRAPH98_STAND_INTERLACE_STAGE_COUNT; ++stage) {
        uint16_t sweep_y;
        uint16_t top_start;
        uint16_t top_end;
        uint16_t bottom_start;
        uint16_t bottom_end;

        sweep_y = (uint16_t)(
            stage * GRAPH98_STAND_INTERLACE_SWEEP_LINES);
        top_end = (uint16_t)(
            sweep_y + GRAPH98_STAND_INTERLACE_GROUP_LINES);
        if (top_end > GRAPH98_STAND_HEIGHT) {
            top_end = GRAPH98_STAND_HEIGHT;
        }

        if (stage == 0u) {
            top_start = 0;
        } else {
            top_start = (uint16_t)(
                sweep_y + GRAPH98_STAND_INTERLACE_GROUP_LINES -
                GRAPH98_STAND_INTERLACE_SWEEP_LINES);
            if (top_start > GRAPH98_STAND_HEIGHT) {
                top_start = GRAPH98_STAND_HEIGHT;
            }
        }
        bottom_start = (uint16_t)(GRAPH98_STAND_HEIGHT - top_end);
        bottom_end = (uint16_t)(GRAPH98_STAND_HEIGHT - top_start);

        graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_BACK);
        graph98_transfer_stand_interlace_stage_plane(
            GRAPH98_VRAM_BLUE, byte_x, (uint16_t)y,
            top_start, top_end, bottom_start, bottom_end,
            GRAPH98_STAND_INTERLACE_PLANE_BYTES * 0u, 0);
        graph98_transfer_stand_interlace_stage_plane(
            GRAPH98_VRAM_RED, byte_x, (uint16_t)y,
            top_start, top_end, bottom_start, bottom_end,
            GRAPH98_STAND_INTERLACE_PLANE_BYTES * 1u, 0);
        graph98_transfer_stand_interlace_stage_plane(
            GRAPH98_VRAM_GREEN, byte_x, (uint16_t)y,
            top_start, top_end, bottom_start, bottom_end,
            GRAPH98_STAND_INTERLACE_PLANE_BYTES * 2u, 0);
        graph98_transfer_stand_interlace_stage_plane(
            GRAPH98_VRAM_INTENS, byte_x, (uint16_t)y,
            top_start, top_end, bottom_start, bottom_end,
            GRAPH98_STAND_INTERLACE_PLANE_BYTES * 3u, 0);

        graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_FRONT);
        graph98_wait_vsync();

        graph98_transfer_stand_interlace_stage_plane(
            GRAPH98_VRAM_BLUE, byte_x, (uint16_t)y,
            top_start, top_end, bottom_start, bottom_end,
            GRAPH98_STAND_INTERLACE_PLANE_BYTES * 0u, 1);
        graph98_transfer_stand_interlace_stage_plane(
            GRAPH98_VRAM_RED, byte_x, (uint16_t)y,
            top_start, top_end, bottom_start, bottom_end,
            GRAPH98_STAND_INTERLACE_PLANE_BYTES * 1u, 1);
        graph98_transfer_stand_interlace_stage_plane(
            GRAPH98_VRAM_GREEN, byte_x, (uint16_t)y,
            top_start, top_end, bottom_start, bottom_end,
            GRAPH98_STAND_INTERLACE_PLANE_BYTES * 2u, 1);
        graph98_transfer_stand_interlace_stage_plane(
            GRAPH98_VRAM_INTENS, byte_x, (uint16_t)y,
            top_start, top_end, bottom_start, bottom_end,
            GRAPH98_STAND_INTERLACE_PLANE_BYTES * 3u, 1);
    }

    ok = 1;

cleanup:
    graph98_restore_default_pages();
    return ok;
}

static void __attribute__((noinline, optimize("Os")))
graph98_transfer_stand_vram_planes(
    uint16_t byte_x,
    uint16_t base_y,
    uint16_t chunk_y,
    uint16_t first_line,
    uint16_t line_count,
    uint16_t plane_bytes,
    int write_to_vram)
{
    uint16_t plane_number;

    for (plane_number = 0;
         plane_number < GRAPH98_STAND_TRANSFER_PLANES;
         ++plane_number) {
        volatile uint8_t __far *plane;
        uint16_t line;
        uint16_t work_start;

        if (plane_number == 0u) {
            plane = GRAPH98_VRAM_BLUE;
        } else if (plane_number == 1u) {
            plane = GRAPH98_VRAM_RED;
        } else if (plane_number == 2u) {
            plane = GRAPH98_VRAM_GREEN;
        } else {
            plane = GRAPH98_VRAM_INTENS;
        }
        work_start = (uint16_t)(plane_bytes * plane_number);

        for (line = first_line;
             line < (uint16_t)(first_line + line_count);
             ++line) {
            uint16_t vram_offset;
            uint16_t work_offset;
            uint16_t i;

            vram_offset = (uint16_t)(
                (base_y + chunk_y + line) * GRAPH98_BYTES_PER_LINE + byte_x);
            work_offset = (uint16_t)(
                work_start + line * GRAPH98_STAND_BYTES_PER_LINE);

            if (write_to_vram) {
                for (i = 0; i < GRAPH98_STAND_BYTES_PER_LINE; ++i) {
                    plane[(uint16_t)(vram_offset + i)] =
                        graph98_image_work[(uint16_t)(work_offset + i)];
                }
            } else {
                for (i = 0; i < GRAPH98_STAND_BYTES_PER_LINE; ++i) {
                    graph98_image_work[(uint16_t)(work_offset + i)] =
                        plane[(uint16_t)(vram_offset + i)];
                }
            }
        }
    }
}

int __attribute__((optimize("Os")))
graph98_draw_stand_file_trans_vram(
    const char *background_path, const char *sprite_path,
    int x, int y, unsigned char transparent_color)
{
    uint16_t byte_x;
    uint16_t chunk_y;
    int first_write;
    int ok;

    ok = 0;

    /* The displayed page remains page 0, including during preparation. */
    graph98_out8(GRAPH98_PORT_DISPLAY_PAGE, GRAPH98_PAGE_FRONT);

    if (background_path == 0 || background_path[0] == '\0') {
        debug_log("stand vram background missing: x=%d", x);
        goto cleanup;
    }

    if (x < 0 || y < 0 ||
        x + (int)GRAPH98_STAND_WIDTH > GRAPH98_WIDTH ||
        y + (int)GRAPH98_STAND_HEIGHT > GRAPH98_HEIGHT ||
        (x & 7) != 0) {
        debug_log("stand vram rect invalid: x=%d y=%d", x, y);
        goto cleanup;
    }

    byte_x = (uint16_t)(x >> 3);
    graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_BACK);

    /* All G98 and SPR file I/O, and all transparency work, ends here. */
    if (!graph98_load_g98_rect(
            background_path,
            x, y,
            x + (int)GRAPH98_STAND_WIDTH - 1,
            y + (int)GRAPH98_STAND_HEIGHT - 1)) {
        debug_log("stand vram background load failed: %s x=%d",
                  background_path, x);
        goto cleanup;
    }

    if (sprite_path != 0 && sprite_path[0] != '\0' &&
        !graph98_draw_sprite_file_trans(
            sprite_path, x, y, transparent_color)) {
        debug_log("stand vram sprite load failed: %s x=%d",
                  sprite_path, x);
        goto cleanup;
    }

    first_write = 1;
    chunk_y = 0;
    while (chunk_y < GRAPH98_STAND_HEIGHT) {
        uint16_t chunk_lines;
        uint16_t plane_bytes;
        uint16_t unit_start;

        chunk_lines = (uint16_t)(GRAPH98_STAND_HEIGHT - chunk_y);
        if (chunk_lines > GRAPH98_STAND_TRANSFER_CHUNK_LINES) {
            chunk_lines = GRAPH98_STAND_TRANSFER_CHUNK_LINES;
        }
        plane_bytes = (uint16_t)(
            chunk_lines * GRAPH98_STAND_BYTES_PER_LINE);

        if (chunk_y != 0u) {
            graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_BACK);
        }

        graph98_transfer_stand_vram_planes(
            byte_x, (uint16_t)y, chunk_y,
            0, chunk_lines, plane_bytes, 0);

        graph98_out8(GRAPH98_PORT_ACCESS_PAGE, GRAPH98_PAGE_FRONT);
        if (first_write) {
            graph98_wait_vsync();
            first_write = 0;
        }

        for (unit_start = 0; unit_start < chunk_lines;
             unit_start = (uint16_t)(
                 unit_start + GRAPH98_STAND_TRANSFER_LINES)) {
            uint16_t unit_lines;

            unit_lines = (uint16_t)(chunk_lines - unit_start);
            if (unit_lines > GRAPH98_STAND_TRANSFER_LINES) {
                unit_lines = GRAPH98_STAND_TRANSFER_LINES;
            }

            /* Complete B/R/G/I for each fixed eight-line unit. */
            graph98_transfer_stand_vram_planes(
                byte_x, (uint16_t)y, chunk_y,
                unit_start, unit_lines, plane_bytes, 1);
        }

        chunk_y = (uint16_t)(chunk_y + chunk_lines);
    }

    ok = 1;

cleanup:
    graph98_restore_default_pages();
    return ok;
}


int graph98_draw_sprite_file_trans(const char *path, int x, int y,
                                   unsigned char transparent_color)
{
    struct graph98_sprite_header header;
    FILE *fp;
    uint16_t src_y;
    uint16_t max_chunk_lines;

    fp = fopen(path, "rb");
    if (fp == 0) {
        return 0;
    }

    if (!graph98_read_sprite_header(fp, &header)) {
        fclose(fp);
        return 0;
    }

    if (!graph98_is_valid_sprite_header(&header)) {
        fclose(fp);
        return 0;
    }

    max_chunk_lines =
        (uint16_t)(GRAPH98_IMAGE_WORK_SIZE / header.width);

    transparent_color &= 0x0Fu;

    src_y = 0;
    while (src_y < header.height) {
        uint16_t chunk_lines;
        uint16_t line;

        chunk_lines = (uint16_t)(header.height - src_y);
        if (chunk_lines > max_chunk_lines) {
            chunk_lines = max_chunk_lines;
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

static void __attribute__((noinline, optimize("Os")))
graph98_draw_status_digit(int x, int y, int digit)
{
    uint16_t line;

    for (line = 0; line < GRAPH98_MONEY_SPRITE_HEIGHT; ++line) {
        graph98_draw_sprite_line_trans_fast(
            graph98_image_work + line * GRAPH98_MONEY_SPRITE_WIDTH,
            digit * GRAPH98_MONEY_DIGIT_WIDTH,
            GRAPH98_MONEY_DIGIT_WIDTH,
            x, y + line, 0);
    }
}

int __attribute__((optimize("Os")))
graph98_draw_status_digits_file(const char *path,
                                int time_x, int time_y,
                                int money_x, int money_y,
                                int hour, int minute, int money)
{
    struct graph98_sprite_header header;
    FILE *fp;

    if (path == 0 || path[0] == '\0' ||
        hour < 0 || hour > 99 || minute < 0 || minute > 99 ||
        money < 0 || money > 32767 ||
        time_x < 0 ||
        time_x > GRAPH98_WIDTH - (int)(GRAPH98_MONEY_DIGIT_WIDTH *
                                        GRAPH98_MONEY_DIGIT_COUNT) ||
        money_x < 0 ||
        money_x > GRAPH98_WIDTH - (int)(GRAPH98_MONEY_DIGIT_WIDTH *
                                         GRAPH98_MONEY_DIGIT_COUNT) ||
        time_y < 0 ||
        time_y > GRAPH98_HEIGHT - (int)GRAPH98_MONEY_SPRITE_HEIGHT ||
        money_y < 0 ||
        money_y > GRAPH98_HEIGHT - (int)GRAPH98_MONEY_SPRITE_HEIGHT) {
        return 0;
    }

    fp = fopen(path, "rb");
    if (fp == 0) {
        return 0;
    }

    if (!graph98_read_sprite_header(fp, &header) ||
        !graph98_is_valid_sprite_header(&header) ||
        header.width != GRAPH98_MONEY_SPRITE_WIDTH ||
        header.height != GRAPH98_MONEY_SPRITE_HEIGHT ||
        fread(graph98_image_work, 1u, GRAPH98_MONEY_SPRITE_BYTES, fp) !=
            GRAPH98_MONEY_SPRITE_BYTES) {
        fclose(fp);
        return 0;
    }
    fclose(fp);

    graph98_draw_status_digit(time_x, time_y, hour / 10);
    graph98_draw_status_digit(time_x + GRAPH98_MONEY_DIGIT_WIDTH,
                              time_y, hour % 10);
    graph98_draw_status_digit(time_x + GRAPH98_MONEY_DIGIT_WIDTH * 3,
                              time_y, minute / 10);
    graph98_draw_status_digit(time_x + GRAPH98_MONEY_DIGIT_WIDTH * 4,
                              time_y, minute % 10);

    money_x += GRAPH98_MONEY_DIGIT_WIDTH *
               ((int)GRAPH98_MONEY_DIGIT_COUNT - 1);
    do {
        graph98_draw_status_digit(money_x, money_y, money % 10);
        money /= 10;
        money_x -= GRAPH98_MONEY_DIGIT_WIDTH;
    } while (money != 0);

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
