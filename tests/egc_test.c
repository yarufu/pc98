#include "egc98.h"

#include <stdint.h>
#include <stdio.h>

#define TEST_SCREEN_WIDTH 640u
#define TEST_SCREEN_HEIGHT 400u
#define TEST_BYTES_PER_LINE 80u
#define TEST_PLANE_SIZE ((uint16_t)(TEST_BYTES_PER_LINE * TEST_SCREEN_HEIGHT))

#define TEST_VRAM_BLUE   ((volatile uint8_t __far *)0xA8000000UL)
#define TEST_VRAM_RED    ((volatile uint8_t __far *)0xB0000000UL)
#define TEST_VRAM_GREEN  ((volatile uint8_t __far *)0xB8000000UL)
#define TEST_VRAM_INTENS ((volatile uint8_t __far *)0xE0000000UL)

static void test_out8(uint16_t port, uint8_t value)
{
    __asm__ __volatile__(
        "outb %%al, %%dx"
        :
        : "a"(value), "d"(port)
        : "cc");
}

static void test_set_graphics_mode(void)
{
    __asm__ __volatile__(
        "movb $0x42, %%ah\n\t"
        "movb $0xc0, %%ch\n\t"
        "int $0x18"
        :
        :
        : "ax", "cx", "cc", "memory");

    __asm__ __volatile__(
        "movb $0x40, %%ah\n\t"
        "int $0x18"
        :
        :
        : "ax", "cc", "memory");

    test_out8(0x006Au, 0x01u);
}

static void test_clear_direct(void)
{
    uint16_t i;

    for (i = 0; i < TEST_PLANE_SIZE; ++i) {
        TEST_VRAM_BLUE[i] = 0u;
        TEST_VRAM_RED[i] = 0u;
        TEST_VRAM_GREEN[i] = 0u;
        TEST_VRAM_INTENS[i] = 0u;
    }
}

static void test_boxfill_direct(uint16_t x, uint16_t y,
                                uint16_t width, uint16_t height,
                                uint8_t color)
{
    uint16_t row;
    uint16_t byte_x;
    uint16_t byte_width;
    uint8_t blue;
    uint8_t red;
    uint8_t green;
    uint8_t intens;

    if (width == 0u || height == 0u ||
        (x & 7u) != 0u || (width & 7u) != 0u ||
        x >= TEST_SCREEN_WIDTH || y >= TEST_SCREEN_HEIGHT ||
        width > (uint16_t)(TEST_SCREEN_WIDTH - x) ||
        height > (uint16_t)(TEST_SCREEN_HEIGHT - y)) {
        return;
    }

    color &= 0x0Fu;
    blue = (color & 0x01u) ? 0xFFu : 0x00u;
    red = (color & 0x02u) ? 0xFFu : 0x00u;
    green = (color & 0x04u) ? 0xFFu : 0x00u;
    intens = (color & 0x08u) ? 0xFFu : 0x00u;
    byte_width = (uint16_t)(width >> 3);

    for (row = 0; row < height; ++row) {
        uint16_t offset;

        offset = (uint16_t)((uint16_t)((y + row) * TEST_BYTES_PER_LINE) +
                            (uint16_t)(x >> 3));
        for (byte_x = 0; byte_x < byte_width; ++byte_x) {
            uint16_t p;

            p = (uint16_t)(offset + byte_x);
            TEST_VRAM_BLUE[p] = blue;
            TEST_VRAM_RED[p] = red;
            TEST_VRAM_GREEN[p] = green;
            TEST_VRAM_INTENS[p] = intens;
        }
    }
}

static void test_wait_key(void)
{
    fflush(stdout);
    __asm__ __volatile__(
        "movb $0x08, %%ah\n\t"
        "int $0x21"
        :
        :
        : "ax", "cc", "memory");
}

static void test_print_bios_flags(const struct egc98_bios_flags *flags)
{
    printf("EGCTEST.EXE - aligned EGC test\n");
    printf("BIOS 0000:054D = %02Xh, bit 6 = %s\n",
           (unsigned)flags->prxdupd_054d,
           (flags->prxdupd_054d & 0x40u) ? "ON" : "OFF");
    printf("BIOS 0000:0597 = %02Xh, bit 2 = %s (reference)\n",
           (unsigned)flags->crt_bios_0597,
           (flags->crt_bios_0597 & 0x04u) ? "ON" : "OFF");
}

static int test_egc_draws(void)
{
    uint8_t color;

    /* First minimal EGC write, immediately followed by conventional VRAM. */
    if (!egc98_boxfill_aligned16(32u, 176u, 16u, 1u, 15u)) {
        return 0;
    }
    test_boxfill_direct(64u, 176u, 16u, 1u, 15u);

    /* Explicit per-line restart: 16x16 and 32x16. */
    if (!egc98_boxfill_aligned16(32u, 192u, 16u, 16u, 12u) ||
        !egc98_boxfill_aligned16(64u, 192u, 32u, 16u, 10u)) {
        return 0;
    }

    /* Same geometry: documented automatic restart versus explicit restart. */
    if (!egc98_boxfill_aligned16_auto_restart_test(
            128u, 192u, 32u, 16u, 14u) ||
        !egc98_boxfill_aligned16(176u, 192u, 32u, 16u, 14u)) {
        return 0;
    }

    /* Vertically separated rectangles, each with a complete begin/end pair. */
    if (!egc98_boxfill_aligned16(576u, 176u, 16u, 16u, 9u) ||
        !egc98_boxfill_aligned16(576u, 208u, 16u, 16u, 11u) ||
        !egc98_boxfill_aligned16(576u, 240u, 16u, 16u, 13u)) {
        return 0;
    }

    /* EGC color row, with a conventional direct-VRAM reference row below. */
    for (color = 0u; color < 16u; ++color) {
        uint16_t x;

        x = (uint16_t)(32u + (uint16_t)color * 32u);
        if (!egc98_boxfill_aligned16(x, 280u, 16u, 16u, color)) {
            return 0;
        }
        test_boxfill_direct(x, 304u, 16u, 16u, color);
    }

    /* Final visible proof that normal four-plane writes work after EGC end. */
    test_boxfill_direct(32u, 344u, 64u, 16u, 15u);
    test_boxfill_direct(48u, 348u, 32u, 8u, 0u);

    return 1;
}

int main(void)
{
    struct egc98_bios_flags flags;

    /* Read-only detection happens before any graphics or EGC I/O access. */
    egc98_read_bios_flags(&flags);

    if ((flags.prxdupd_054d & 0x40u) == 0u) {
        test_print_bios_flags(&flags);
        printf("EGC BIOS FLAG: OFF\n");
        printf("EGC PORT ACCESS: SKIPPED\n");
        printf("Press any key to exit.\n");
        test_wait_key();
        return 0;
    }

    test_set_graphics_mode();
    test_clear_direct();
    test_print_bios_flags(&flags);
    printf("EGC BIOS FLAG: ON\n");
    printf("EGC PORT ACCESS: STARTING\n");
    fflush(stdout);

    if (!test_egc_draws()) {
        printf("EGC TEST: DRAW SKIPPED OR INVALID\n");
        printf("Press any key to exit.\n");
        test_wait_key();
        return 1;
    }

    printf("EGC PORT ACCESS: COMPLETED\n");
    printf("AUTO/EXPLICIT 32x16 blocks should match.\n");
    printf("Color row y=280 must match direct row y=304.\n");
    printf("White frame y=344 is direct VRAM after EGC end.\n");
    printf("Press any key to exit.\n");
    test_wait_key();
    return 0;
}
