#include "egc98.h"

#include <stdint.h>
#include <stdio.h>

#define BENCH_SCREEN_WIDTH 640u
#define BENCH_SCREEN_HEIGHT 400u
#define BENCH_BYTES_PER_LINE 80u
#define BENCH_HUNDREDTHS_PER_DAY 8640000UL
#define BENCH_COLOR 0u
#define BENCH_PASSES 10u

#define BENCH_VRAM_BLUE   ((volatile uint8_t __far *)0xA8000000UL)
#define BENCH_VRAM_RED    ((volatile uint8_t __far *)0xB0000000UL)
#define BENCH_VRAM_GREEN  ((volatile uint8_t __far *)0xB8000000UL)
#define BENCH_VRAM_INTENS ((volatile uint8_t __far *)0xE0000000UL)

struct bench_case {
    uint16_t width;
    uint16_t height;
    uint16_t iterations;
};

static const struct bench_case bench_cases[] = {
    {  16u,  16u, 50000u },
    {  64u,  64u,  3125u },
    { 320u, 200u,   200u },
    { 640u, 400u,    50u }
};

static void bench_out8(uint16_t port, uint8_t value)
{
    __asm__ __volatile__(
        "outb %%al, %%dx"
        :
        : "a"(value), "d"(port)
        : "cc");
}

static void bench_set_graphics_mode(void)
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

    bench_out8(0x006Au, 0x01u);
}

/* Dedicated DIRECT path; it is independent of graph98_boxfill() and USE_EGC. */
static void bench_boxfill_direct(uint16_t x, uint16_t y,
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
        x >= BENCH_SCREEN_WIDTH || y >= BENCH_SCREEN_HEIGHT ||
        width > (uint16_t)(BENCH_SCREEN_WIDTH - x) ||
        height > (uint16_t)(BENCH_SCREEN_HEIGHT - y)) {
        return;
    }

    color &= 0x0Fu;
    blue = (color & 0x01u) ? 0xFFu : 0x00u;
    red = (color & 0x02u) ? 0xFFu : 0x00u;
    green = (color & 0x04u) ? 0xFFu : 0x00u;
    intens = (color & 0x08u) ? 0xFFu : 0x00u;
    byte_width = (uint16_t)(width >> 3);

    for (row = 0u; row < height; ++row) {
        uint16_t offset;

        offset = (uint16_t)((uint16_t)((y + row) *
                                      BENCH_BYTES_PER_LINE) +
                            (uint16_t)(x >> 3));
        for (byte_x = 0u; byte_x < byte_width; ++byte_x) {
            uint16_t p;

            p = (uint16_t)(offset + byte_x);
            BENCH_VRAM_BLUE[p] = blue;
            BENCH_VRAM_RED[p] = red;
            BENCH_VRAM_GREEN[p] = green;
            BENCH_VRAM_INTENS[p] = intens;
        }
    }
}

static uint32_t bench_dos_hundredths(void)
{
    uint16_t time_cx;
    uint16_t time_dx;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t hundredth;

    __asm__ __volatile__(
        "movb $0x2c, %%ah\n\t"
        "int $0x21"
        : "=c"(time_cx), "=d"(time_dx)
        :
        : "ax", "cc", "memory");

    hour = (uint8_t)(time_cx >> 8);
    minute = (uint8_t)time_cx;
    second = (uint8_t)(time_dx >> 8);
    hundredth = (uint8_t)time_dx;

    return (((uint32_t)hour * 60u + minute) * 60u + second) * 100u +
           hundredth;
}

static uint32_t bench_elapsed(uint32_t start, uint32_t end)
{
    if (end >= start) {
        return end - start;
    }
    return (BENCH_HUNDREDTHS_PER_DAY - start) + end;
}

static uint32_t bench_measure_direct(const struct bench_case *item)
{
    uint16_t pass;
    uint16_t iteration;
    uint32_t start;
    uint32_t end;

    start = bench_dos_hundredths();
    for (pass = 0u; pass < BENCH_PASSES; ++pass) {
        for (iteration = 0u; iteration < item->iterations; ++iteration) {
            bench_boxfill_direct(0u, 0u, item->width, item->height,
                                 BENCH_COLOR);
        }
    }
    end = bench_dos_hundredths();

    return bench_elapsed(start, end);
}

static uint32_t bench_measure_egc(const struct bench_case *item)
{
    uint16_t pass;
    uint16_t iteration;
    uint32_t start;
    uint32_t end;

    start = bench_dos_hundredths();
    for (pass = 0u; pass < BENCH_PASSES; ++pass) {
        for (iteration = 0u; iteration < item->iterations; ++iteration) {
            (void)egc98_boxfill_aligned16(0u, 0u,
                                         item->width, item->height,
                                         BENCH_COLOR);
        }
    }
    end = bench_dos_hundredths();

    return bench_elapsed(start, end);
}

static void bench_print_result(const struct bench_case *item,
                               uint32_t direct_time, uint32_t egc_time)
{
    uint32_t speedup_x100;
    uint32_t total_loops;

    total_loops = (uint32_t)item->iterations * (uint32_t)BENCH_PASSES;

    printf("%ux%u (%lu total loops)\n",
           (unsigned)item->width,
           (unsigned)item->height,
           (unsigned long)total_loops);
    printf("DIRECT: %lu ms\n", (unsigned long)(direct_time * 10u));
    printf("EGC:    %lu ms\n", (unsigned long)(egc_time * 10u));

    if (egc_time == 0u) {
        printf("speedup: timer resolution too low\n\n");
        return;
    }

    speedup_x100 = (uint32_t)((direct_time * 100u) / egc_time);
    printf("speedup: %lu.%02lux\n\n",
           (unsigned long)(speedup_x100 / 100u),
           (unsigned long)(speedup_x100 % 100u));
}

static void bench_wait_key(void)
{
    fflush(stdout);
    __asm__ __volatile__(
        "movb $0x08, %%ah\n\t"
        "int $0x21"
        :
        :
        : "ax", "cc", "memory");
}

int main(void)
{
    unsigned int i;

    printf("EGCBENCH.EXE - DIRECT vs EGC\n");
    printf("BIOS EGC FLAG: %s\n\n",
           egc98_bios_flag_present() ? "ON" : "OFF");

    if (!egc98_bios_flag_present()) {
        printf("EGC PORT ACCESS: SKIPPED\n");
        printf("Press any key to exit.\n");
        bench_wait_key();
        return 0;
    }

    fflush(stdout);
    bench_set_graphics_mode();

    for (i = 0u; i < sizeof(bench_cases) / sizeof(bench_cases[0]); ++i) {
        uint32_t direct_time;
        uint32_t egc_time;

        /* Untimed preflight also verifies that the EGC call is accepted. */
        if (egc98_boxfill_aligned16(0u, 0u,
                                    bench_cases[i].width,
                                    bench_cases[i].height,
                                    BENCH_COLOR) != EGC98_DRAW_OK) {
            printf("%ux%u: EGC DRAW SKIPPED\n",
                   (unsigned)bench_cases[i].width,
                   (unsigned)bench_cases[i].height);
            break;
        }

        direct_time = bench_measure_direct(&bench_cases[i]);
        egc_time = bench_measure_egc(&bench_cases[i]);
        bench_print_result(&bench_cases[i], direct_time, egc_time);
    }

    printf("Press any key to exit.\n");
    bench_wait_key();
    return 0;
}
