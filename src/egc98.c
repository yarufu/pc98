#include "egc98.h"

#include <stdint.h>

#define EGC98_SCREEN_WIDTH 640u
#define EGC98_SCREEN_HEIGHT 400u
#define EGC98_BYTES_PER_LINE 80u

#define EGC98_BIOS_PRXDUPD \
    (*(volatile const uint8_t __far *)0x0000054DUL)
#define EGC98_BIOS_CRT_BIOS \
    (*(volatile const uint8_t __far *)0x00000597UL)

#define EGC98_VRAM_BLUE_WORD \
    ((volatile uint16_t __far *)0xA8000000UL)

#define EGC98_PORT_ACTIVE_PLANE 0x04A0u
#define EGC98_PORT_READ_PLANE   0x04A2u
#define EGC98_PORT_MODE_ROP     0x04A4u
#define EGC98_PORT_FG_COLOR     0x04A6u
#define EGC98_PORT_MASK         0x04A8u
#define EGC98_PORT_ADDRESS      0x04ACu
#define EGC98_PORT_BIT_LENGTH   0x04AEu

#define EGC98_ALL_PLANES_ACTIVE 0xFFF0u
#define EGC98_PATTERN_REGISTER  0x00FFu
#define EGC98_FOREGROUND_SELECT 0x40FFu
#define EGC98_SOLID_FG_ROP      0x2CAAu
#define EGC98_FULL_MASK         0xFFFFu
#define EGC98_ALIGNED_ADDRESS   0x0000u
#define EGC98_TRIGGER_WORD      0xFFFFu

static void __attribute__((noinline))
egc98_out16(uint16_t port, uint16_t value)
{
    __asm__ __volatile__(
        "outw %%ax, %%dx"
        :
        : "a"(value), "d"(port)
        : "cc");
}

/*
 * Only the mode-change sequence is protected from interrupts. Interrupts are
 * restored before any rectangle data is written to VRAM.
 */
static void egc98_begin(void)
{
    __asm__ __volatile__(
        "pushf\n\t"
        "cli\n\t"
        "movw $0x007c, %%dx\n\t"
        "xorb %%al, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "movw $0x006a, %%dx\n\t"
        "movb $0x07, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "movb $0x05, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "movw $0x007c, %%dx\n\t"
        "movb $0x80, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "movw $0x006a, %%dx\n\t"
        "movb $0x06, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "popf"
        :
        :
        : "ax", "dx", "cc", "memory");
}

static void egc98_end(void)
{
    /* Restore the register state used by conventional direct VRAM access. */
    egc98_out16(EGC98_PORT_READ_PLANE, EGC98_PATTERN_REGISTER);
    egc98_out16(EGC98_PORT_ACTIVE_PLANE, EGC98_ALL_PLANES_ACTIVE);
    egc98_out16(EGC98_PORT_MASK, EGC98_FULL_MASK);

    __asm__ __volatile__(
        "pushf\n\t"
        "cli\n\t"
        "movw $0x006a, %%dx\n\t"
        "movb $0x07, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "movb $0x04, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "movw $0x007c, %%dx\n\t"
        "xorb %%al, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "movw $0x006a, %%dx\n\t"
        "movb $0x06, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "popf"
        :
        :
        : "ax", "dx", "cc", "memory");
}

void egc98_read_bios_flags(struct egc98_bios_flags *flags)
{
    if (flags == 0) {
        return;
    }

    flags->prxdupd_054d = EGC98_BIOS_PRXDUPD;
    flags->crt_bios_0597 = EGC98_BIOS_CRT_BIOS;
}

int egc98_bios_flag_present(void)
{
    return (EGC98_BIOS_PRXDUPD & 0x40u) != 0u;
}

static int egc98_boxfill_impl(uint16_t x, uint16_t y,
                              uint16_t width, uint16_t height,
                              uint8_t color, int restart_each_line)
{
    uint16_t row;
    uint16_t words_per_line;
    uint16_t first_word;

    if (!egc98_bios_flag_present()) {
        return EGC98_DRAW_SKIPPED;
    }

    if (width == 0u || height == 0u ||
        x >= EGC98_SCREEN_WIDTH || y >= EGC98_SCREEN_HEIGHT ||
        (x & 15u) != 0u || (width & 15u) != 0u ||
        width > (uint16_t)(EGC98_SCREEN_WIDTH - x) ||
        height > (uint16_t)(EGC98_SCREEN_HEIGHT - y)) {
        return EGC98_DRAW_SKIPPED;
    }

    words_per_line = (uint16_t)(width >> 4);
    first_word = (uint16_t)(((uint16_t)(y * EGC98_BYTES_PER_LINE) +
                             (uint16_t)(x >> 3)) >> 1);

    egc98_begin();

    egc98_out16(EGC98_PORT_ACTIVE_PLANE, EGC98_ALL_PLANES_ACTIVE);
    egc98_out16(EGC98_PORT_READ_PLANE, EGC98_PATTERN_REGISTER);
    egc98_out16(EGC98_PORT_MASK, EGC98_FULL_MASK);
    egc98_out16(EGC98_PORT_READ_PLANE, EGC98_FOREGROUND_SELECT);
    egc98_out16(EGC98_PORT_MODE_ROP, EGC98_SOLID_FG_ROP);
    egc98_out16(EGC98_PORT_FG_COLOR, (uint16_t)(color & 0x0Fu));

    if (!restart_each_line) {
        egc98_out16(EGC98_PORT_ADDRESS, EGC98_ALIGNED_ADDRESS);
        egc98_out16(EGC98_PORT_BIT_LENGTH, (uint16_t)(width - 1u));
    }

    for (row = 0; row < height; ++row) {
        uint16_t word;
        uint16_t row_word;

        if (restart_each_line) {
            egc98_out16(EGC98_PORT_ADDRESS, EGC98_ALIGNED_ADDRESS);
            egc98_out16(EGC98_PORT_BIT_LENGTH, (uint16_t)(width - 1u));
        }

        row_word = (uint16_t)(first_word +
                              row * (EGC98_BYTES_PER_LINE / 2u));
        for (word = 0; word < words_per_line; ++word) {
            EGC98_VRAM_BLUE_WORD[(uint16_t)(row_word + word)] =
                EGC98_TRIGGER_WORD;
        }
    }

    egc98_end();
    return EGC98_DRAW_OK;
}

int egc98_boxfill_aligned16(uint16_t x, uint16_t y,
                           uint16_t width, uint16_t height,
                           uint8_t color)
{
    return egc98_boxfill_impl(x, y, width, height, color, 1);
}

int egc98_boxfill_aligned16_auto_restart_test(uint16_t x, uint16_t y,
                                             uint16_t width, uint16_t height,
                                             uint8_t color)
{
    return egc98_boxfill_impl(x, y, width, height, color, 0);
}
